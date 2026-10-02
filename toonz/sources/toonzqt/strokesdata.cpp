

#include "toonzqt/strokesdata.h"
#include "tthreadmessage.h"
#include "tstroke.h"
#include "tpalette.h"
#include "toonzqt/rasterimagedata.h"
#include "toonz/toonzimageutils.h"
#include "toonz/trasterimageutils.h"
#include "toonz/stage.h"
#include "toonzqt/gutil.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tvectorgl.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QOpenGLContext>
#include <QPointer>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace std;

namespace {
const char *const VectorClipboardFormat =
    "application/x-opentoonz-vector-selection-v2";
const char VectorClipboardMagic[]       = {'O', 'T', 'V', '2'};
constexpr int VectorClipboardHeaderSize = 4 + 4 + 32;
constexpr int MaxVectorClipboardSize    = 64 * 1024 * 1024;

constexpr int MaxVectorClipboardPreviewSize = 4096;

// TOfflineGL can replace the current context while constructing or destroying
// its resources. Keep this guard outside its lifetime, including on failure.
class ClipboardGLContextGuard final {
  QPointer<QOpenGLContext> m_context;
  QSurface *m_surface;

public:
  ClipboardGLContextGuard()
      : m_context(QOpenGLContext::currentContext())
      , m_surface(m_context ? m_context->surface() : nullptr) {}

  ~ClipboardGLContextGuard() {
    if (m_context && m_surface) {
      if (!m_context->makeCurrent(m_surface))
        qWarning("Could not restore the OpenGL context after clipboard preview.");
    } else if (QOpenGLContext *current = QOpenGLContext::currentContext()) {
      current->doneCurrent();
    }
  }

  ClipboardGLContextGuard(const ClipboardGLContextGuard &) = delete;
  ClipboardGLContextGuard &operator=(const ClipboardGLContextGuard &) = delete;
};

QImage vectorClipboardImage(const TVectorImageP &image) {
  const TRectD bounds = image->getBBox();
  if (bounds.isEmpty() || !std::isfinite(bounds.x0) ||
      !std::isfinite(bounds.y0) || !std::isfinite(bounds.x1) ||
      !std::isfinite(bounds.y1))
    return QImage();

  // Bound the allocation before converting to integer pixel dimensions. Only
  // the rendered preview is scaled; the native strokes and PLI stay unchanged.
  const double x0     = std::floor(bounds.x0);
  const double y0     = std::floor(bounds.y0);
  const double width  = std::ceil(bounds.x1) - x0 + 1.0;
  const double height = std::ceil(bounds.y1) - y0 + 1.0;
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0 ||
      height <= 0.0)
    return QImage();

  const double scale =
      std::min(1.0, MaxVectorClipboardPreviewSize / std::max(width, height));
  const int lx = std::max(
      1, std::min(MaxVectorClipboardPreviewSize, int(std::ceil(width * scale))));
  const int ly = std::max(
      1, std::min(MaxVectorClipboardPreviewSize, int(std::ceil(height * scale))));
  const TDimension size(lx, ly);

  ClipboardGLContextGuard contextGuard;
  TOfflineGL offline(size);
  offline.clear(TPixel32(0, 0, 0, 0));
  TVectorRenderData rd(TScale(scale) * TTranslation(-x0, -y0), TRect(size),
                       image->getPalette(), 0, true, true);
  rd.m_drawRegions = true;
  offline.draw(image, rd, false);
  // Copy the borrowed pixels while the raster and offscreen resources live.
  const TRaster32P raster = offline.getRaster();
  return raster ? rasterToQImage(raster).copy() : QImage();
}

QByteArray makeVectorClipboardPayload(const QByteArray &pliData) {
  QByteArray payload;
  payload.reserve(VectorClipboardHeaderSize + pliData.size());
  payload.append(VectorClipboardMagic, 4);
  const quint32 size = static_cast<quint32>(pliData.size());
  payload.append(static_cast<char>((size >> 24) & 0xff));
  payload.append(static_cast<char>((size >> 16) & 0xff));
  payload.append(static_cast<char>((size >> 8) & 0xff));
  payload.append(static_cast<char>(size & 0xff));
  payload.append(QCryptographicHash::hash(pliData, QCryptographicHash::Sha256));
  payload.append(pliData);
  return payload;
}

QByteArray vectorPliData(const QByteArray &payload) {
  if (payload.size() < VectorClipboardHeaderSize ||
      memcmp(payload.constData(), VectorClipboardMagic, 4) != 0)
    return QByteArray();

  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(payload.constData());
  const quint32 size = (static_cast<quint32>(data[4]) << 24) |
                       (static_cast<quint32>(data[5]) << 16) |
                       (static_cast<quint32>(data[6]) << 8) |
                       static_cast<quint32>(data[7]);
  if (size == 0 || size > MaxVectorClipboardSize ||
      payload.size() != VectorClipboardHeaderSize + static_cast<int>(size))
    return QByteArray();

  const QByteArray pliData      = payload.mid(VectorClipboardHeaderSize);
  const QByteArray expectedHash = payload.mid(8, 32);
  if (QCryptographicHash::hash(pliData, QCryptographicHash::Sha256) !=
      expectedHash)
    return QByteArray();
  return pliData;
}
}  // namespace

void StrokesData::setClipboardFormats() {
  if (!m_image || m_image->getStrokeCount() == 0) return;
  try {
    const QImage preview = vectorClipboardImage(m_image);
    if (!preview.isNull()) setImageData(preview);
  } catch (...) {
    // Native copy remains available when a rendering context cannot be made.
  }

  QTemporaryDir dir;
  if (!dir.isValid()) return;
  QString path = dir.filePath("selection.pli");
  try {
    // A split selection can retain internal state from its source image that
    // is not suitable for a standalone PLI. Rebuild it just as the custom
    // vector-brush exporter does before handing it to the PLI writer.
    TPaletteP palette           = new TPalette();
    TVectorImageP sourceCopy    = m_image->clone();
    TVectorImageP transferImage = new TVectorImage();
    transferImage->setPalette(palette.getPointer());
    transferImage->setAutocloseTolerance(m_image->getAutocloseTolerance());
    transferImage->mergeImage(sourceCopy, TAffine());
    transferImage->findRegions();
    if (transferImage->getStrokeCount() == 0) return;

    TLevelP level = new TLevel();
    level->setPalette(palette.getPointer());
    level->setFrame(TFrameId(1), transferImage);
    TLevelWriterP writer(TFilePath(path.toStdWString()));
    writer->save(level);
    writer = TLevelWriterP();  // The PLI writer finishes on destruction.
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
      const QByteArray pliData = file.readAll();
      if (!pliData.isEmpty() && pliData.size() <= MaxVectorClipboardSize)
        QMimeData::setData(VectorClipboardFormat,
                           makeVectorClipboardPayload(pliData));
    }
  } catch (...) {
    // The image representation and in-process vector data still work.
  }
}

StrokesData *StrokesData::fromClipboard(const QMimeData *mime) {
  if (!mime || !mime->hasFormat(VectorClipboardFormat)) return nullptr;
  const QByteArray bytes = vectorPliData(mime->data(VectorClipboardFormat));
  if (bytes.isEmpty()) return nullptr;
  QTemporaryDir dir;
  if (!dir.isValid()) return nullptr;
  QString path = dir.filePath("selection.pli");
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
    return nullptr;
  file.close();
  try {
    TLevelReaderP reader(TFilePath(path.toStdWString()));
    // PLI frame readers need loadInfo() to initialize the stream and palette.
    TLevelP level = reader->loadInfo();
    if (!level || level->getFrameCount() != 1) return nullptr;
    TImageReaderP frame = reader->getFrameReader(TFrameId(1));
    TImageP loaded      = frame ? frame->load() : TImageP();
    TVectorImageP image = loaded;
    if (!image || image->getStrokeCount() == 0 || !level->getPalette() ||
        level->getPalette()->getPageCount() == 0 ||
        level->getPalette()->getStyleCount() == 0)
      return nullptr;
    image->setPalette(level->getPalette());
    return new StrokesData(image.getPointer());
  } catch (...) {
    return nullptr;
  }
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

int findStroke(const TVectorImageP &img, TStroke *stroke, const TAffine &aff) {
  TRectD strokeBBox = aff * stroke->getBBox();
  int count         = img->getStrokeCount();
  for (int i = 0; i < count; i++) {
    TStroke *s  = img->getStroke(i);
    TRectD bbox = s->getBBox();
    if (tdistance2(bbox.getP00(), strokeBBox.getP00()) +
            tdistance2(bbox.getP11(), strokeBBox.getP11()) >
        0.001)
      continue;
    return i;
  }
  return -1;
}

//-----------------------------------------------------------------------------

TAffine findOffset(const TVectorImageP &srcImg, const TVectorImageP &img) {
  TAffine offset;

  TVectorImageP tarImg = img;
  if (!tarImg) return offset;
  if (tarImg->getStrokeCount() == 0 || srcImg->getStrokeCount() == 0)
    return offset;
  bool done = false;
  int i;
  while (!done)
    for (i = 0; i < (int)srcImg->getStrokeCount(); i++) {
      TStroke *stroke = srcImg->getStroke(i);
      assert(stroke);
      if (findStroke(tarImg, stroke, offset) >= 0) {
        offset = offset * TTranslation(10, -10);
        break;
      }
      done = true;
    }
  return offset;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

TStroke getStrokeByRect(TRectD r) {
  TStroke stroke;
  if (r.isEmpty()) return stroke;
  vector<TThickPoint> points;
  points.push_back(r.getP00());
  points.push_back((r.getP00() + r.getP01()) * 0.5);
  points.push_back(r.getP01());
  points.push_back((r.getP01() + r.getP11()) * 0.5);
  points.push_back(r.getP11());
  points.push_back((r.getP11() + r.getP10()) * 0.5);
  points.push_back(r.getP10());
  points.push_back((r.getP10() + r.getP00()) * 0.5);
  points.push_back(r.getP00());
  stroke.reshape(&(points[0]), points.size());
  stroke.setSelfLoop(true);
  return stroke;
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// StrokesData
//-----------------------------------------------------------------------------

void StrokesData::setImage(TVectorImageP image, const std::set<int> &indices) {
  if (!image) return;
  if (indices.empty()) return;

  // indices e' un set; splitImage si aspetta un vector
  vector<int> indicesV(indices.begin(), indices.end());
  QMutexLocker lock(image->getMutex());
  m_image = image->splitImage(indicesV, false);
  if (m_image->getPalette() == 0) {
    // nel caso lo stroke sia un path (e quindi senza palette)
    m_image->setPalette(new TPalette());
  }
}

//-----------------------------------------------------------------------------

void StrokesData::getImage(TVectorImageP image, std::set<int> &indices,
                           bool insert) const {
  if (!m_image) return;

  TVectorImageP srcImg = m_image;

  QMutexLocker lock(image->getMutex());
  if (insert) {
    TAffine offset    = findOffset(srcImg, image);
    UINT oldImageSize = image->getStrokeCount();

    int insertAt = image->mergeImage(srcImg, offset, false);
    UINT newImageSize = image->getStrokeCount();
    indices.clear();
    if (insertAt == 0)
        for (UINT sI = oldImageSize; sI < newImageSize; sI++) indices.insert(sI);
    else
        for (UINT sI = oldImageSize; sI < newImageSize; sI++)
            indices.insert(sI - oldImageSize + insertAt);
  } else {
    std::vector<int> indicesToInsert(indices.begin(), indices.end());
    if (indicesToInsert.empty()) return;
    image->insertImage(srcImg, indicesToInsert);
  }
}

//-----------------------------------------------------------------------------

ToonzImageData *StrokesData::toToonzImageData(
    const TToonzImageP &imageToPaste) const {
  double dpix, dpiy;
  imageToPaste->getDpi(dpix, dpiy);
  assert(dpix != 0 && dpiy != 0);
  TScale sc(dpix / Stage::inch, dpiy / Stage::inch);

  TRectD bbox = sc * m_image->getBBox();
  bbox.x0     = tfloor(bbox.x0);
  bbox.y0     = tfloor(bbox.y0);
  bbox.x1     = tceil(bbox.x1);
  bbox.y1     = tceil(bbox.y1);
  TDimension size(bbox.getLx(), bbox.getLy());
  TToonzImageP app = ToonzImageUtils::vectorToToonzImage(
      m_image, sc, m_image->getPalette(), bbox.getP00(), size, 0, true);

  vector<TRectD> rects;
  vector<TStroke> strokes;
  TStroke stroke = getStrokeByRect(bbox);
  strokes.push_back(stroke);
  ToonzImageData *data = new ToonzImageData();
  data->setData(app->getRaster(), m_image->getPalette(), dpix, dpiy,
                TDimension(), rects, strokes, strokes, TAffine());
  return data;
}

//-----------------------------------------------------------------------------

FullColorImageData *StrokesData::toFullColorImageData(
    const TRasterImageP &imageToPaste) const {
  double dpix, dpiy;
  imageToPaste->getDpi(dpix, dpiy);
  assert(dpix != 0 && dpiy != 0);
  TScale sc(dpix / Stage::inch, dpiy / Stage::inch);

  TRectD bbox = sc * m_image->getBBox();
  bbox.x0     = tfloor(bbox.x0);
  bbox.y0     = tfloor(bbox.y0);
  bbox.x1     = tceil(bbox.x1);
  bbox.y1     = tceil(bbox.y1);
  TDimension size(bbox.getLx(), bbox.getLy());
  TRasterImageP app = TRasterImageUtils::vectorToFullColorImage(
      m_image, sc, m_image->getPalette(), bbox.getP00(), size, 0, true);

  vector<TRectD> rects;
  vector<TStroke> strokes;
  TStroke stroke = getStrokeByRect(bbox);
  strokes.push_back(stroke);
  FullColorImageData *data = new FullColorImageData();
  data->setData(app->getRaster(), m_image->getPalette(), dpix, dpiy,
                TDimension(), rects, strokes, strokes, TAffine());
  return data;
}
