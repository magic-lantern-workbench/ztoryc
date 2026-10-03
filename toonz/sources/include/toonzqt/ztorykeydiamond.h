#pragma once

// =============================================================================
// Grammatica del diamante chiave (Ztoryc) — SORGENTE UNICA
// -----------------------------------------------------------------------------
// Il diamante porta due assi indipendenti in un solo glifo:
//
//   meta' destra vuota  = chiave PARZIALE
//   meta' sinistra      = quali sistemi: bianco trasformazione, oro posa, o
//                         bianco-sopra/oro-sotto quando un parziale tiene
//                         entrambi.
//
//   [] bianco pieno            trasformazione completa
//   [| bianco/vuoto            trasformazione parziale
//   [] bianco | oro            chiavi su tutto (chiave "All")
//   [] oro pieno               posa completa
//   [| bianco-su-oro / vuoto   entrambi parziali
//   oro | bianco-sopra/vuoto   posa completa, trasformazione parziale
//   bianco | oro-sopra/vuoto   trasformazione completa, posa parziale
//   [| oro / vuoto             solo posa, parziale
//
// Le ultime tre dal 2026-10-03 (Franco): prima ricadevano in oro pieno e in
// bianco|oro, e lo stesso glifo diceva due cose diverse. La meta' destra ora
// ha due quarti: quello in basso vuoto segna sempre "qualcosa e' parziale".
//
// Sta qui, e non nei due chiamanti, perche' lo xsheet (CellArea::drawKeyframe)
// e il KeyframeNavigator del viewer devono restare la STESSA lingua: se
// divergono, l'utente vede due verita' diverse sullo stesso frame. Vale sia per
// i colori sia per il rilevamento dello stato — il bug storico
// ("ogni chiave di posa sembra parziale") stava proprio nel rilevamento.
//
// Header-only per non toccare il CMake. Usato da toonz (xsheet) e toonzqt
// (navigator), entrambi linkano tnzext.
// =============================================================================

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

#include "toonzqt/ztorytheme.h"
#include "toonz/tstageobject.h"
#include "tdoubleparam.h"
#include "ext/plasticskeletondeformation.h"

namespace ZtoryTheme {

struct KeyDiamond {
  // QColor() invalido = regione vuota. La meta' destra ha due quarti.
  QColor leftTop, leftBottom, right, rightBottom;
};

//! \p stageAny / \p stageFull: la trasformazione di colonna ha una / tutte
//! le chiavi dei canali. \p plasticAny / \p plasticFull: la posa plastic ha
//! almeno una / tutte le deformazioni di vertice con chiavi. Presuppone che una
//! chiave ci sia (il diamante si disegna solo su un frame che ha una chiave).
inline KeyDiamond keyDiamond(bool stageAny, bool stageFull, bool plasticAny,
                             bool plasticFull) {
  const QColor w = Qt::white;
  const QColor g = gold();
  const QColor o = QColor();  // vuoto
  if (!plasticAny)  // Solo trasformazione.
    return stageFull ? KeyDiamond{w, w, w, w} : KeyDiamond{w, w, o, o};
  if (stageFull)
    return plasticFull ? KeyDiamond{w, w, g, g}   // chiave su tutto
                       : KeyDiamond{w, w, g, o};  // posa parziale
  if (plasticFull)
    return stageAny ? KeyDiamond{g, g, w, o}   // trasformazione parziale
                    : KeyDiamond{g, g, g, g};  // solo posa, completa
  return stageAny ? KeyDiamond{w, g, o, o}   // entrambi parziali
                  : KeyDiamond{g, g, o, o};  // solo posa, parziale
}

//! Almeno un canale della trasformazione ha una chiave alla riga \p row —
//! senza contare la posa, che TStageObject::isKeyframe puo' includere.
inline bool stageTransformAny(TStageObject *pegbar, int row) {
  if (!pegbar) return false;
  static const TStageObject::Channel kChannels[] = {
      TStageObject::T_Angle,  TStageObject::T_X,      TStageObject::T_Y,
      TStageObject::T_Z,      TStageObject::T_SO,     TStageObject::T_ScaleX,
      TStageObject::T_ScaleY, TStageObject::T_Scale,  TStageObject::T_Path,
      TStageObject::T_ShearX, TStageObject::T_ShearY};
  for (TStageObject::Channel ch : kChannels)
    if (TDoubleParam *p = pegbar->getParam(ch))
      if (p->isKeyframe(row)) return true;
  return false;
}

//! Tutte e tre le regioni dello stesso colore (selezione, o marker semplice).
inline KeyDiamond keyDiamondSolid(const QColor &c) { return {c, c, c, c}; }

//! Stato della posa plastic di \p pegbar alla riga \p row.
//! \p any = almeno una deformazione di vertice ha una chiave,
//! \p full = le hanno tutte.
//!
//! NON PlasticSkeletonDeformation::isFullKeyframe(): quella pretende anche il
//! parametro skeleton-ids, che NESSUNO dei due percorsi che mettono una chiave
//! di posa tocca (il Plastic tool lo dice esplicitamente, e
//! TStageObject::setPlasticPoseKeyframe cammina solo i parametri di posa).
//! Usandola, ogni chiave di posa risultava parziale.
//!
//! Nota paramsTime(), non la riga grezza: i parametri plastic sono campionati
//! nel tempo-parametri dello stage object, che diverge dalla riga xsheet quando
//! c'e' un ciclo.
inline void plasticPoseState(TStageObject *pegbar, int row, bool &any,
                             bool &full) {
  any = full = false;
  if (!pegbar) return;
  const PlasticSkeletonDeformationP &psd =
      pegbar->getPlasticSkeletonDeformation();
  if (!psd) return;

  const double pf = pegbar->paramsTime(row);
  any             = psd->isKeyframe(pf);
  if (!any) return;

  full = true;
  PlasticSkeletonDeformation::vd_iterator vdt, vdEnd;
  psd->vertexDeformations(vdt, vdEnd);
  for (; vdt != vdEnd; ++vdt)
    if (!(*vdt).second->isFullKeyframe(pf)) {
      full = false;
      break;
    }
}

//! Il diamante che compete a \p pegbar alla riga \p row. Il chiamante ha gia'
//! verificato che il frame abbia una chiave.
inline KeyDiamond keyDiamondForStageObject(TStageObject *pegbar, int row) {
  bool any = false, full = false;
  plasticPoseState(pegbar, row, any, full);
  return keyDiamond(stageTransformAny(pegbar, row),
                    pegbar && pegbar->isFullKeyframe(row), any, full);
}

//! Riempie le tre regioni di \p path: meta' sinistra divisa in alto/basso,
//! meta' destra intera. I rect di clip sono arrotondati verso l'esterno cosi'
//! nessun pixel di cucitura tra le regioni resta non dipinto.
inline void fillKeyRegions(QPainter &p, const QPainterPath &path,
                           const KeyDiamond &d) {
  const QRectF bb  = path.boundingRect();
  const qreal midX = bb.center().x();
  const qreal midY = bb.center().y();

  auto fillRegion = [&](const QColor &fill, const QRectF &clip) {
    if (!fill.isValid()) return;  // vuota: si vede solo il contorno comune
    p.save();
    p.setClipRect(clip.adjusted(-1.0, -1.0, 1.0, 1.0), Qt::IntersectClip);
    p.fillPath(path, QBrush(fill));
    p.restore();
  };

  const qreal lw = midX - bb.left(), rw = bb.right() - midX;
  const qreal th = midY - bb.top(), bh = bb.bottom() - midY;
  fillRegion(d.leftTop, QRectF(bb.left(), bb.top(), lw, th));
  fillRegion(d.leftBottom, QRectF(bb.left(), midY, lw, bh));
  fillRegion(d.right, QRectF(midX, bb.top(), rw, th));
  fillRegion(d.rightBottom, QRectF(midX, midY, rw, bh));
}

//! Diamante inscritto in \p r (punte sugli assi verticale/orizzontale).
inline QPainterPath keyDiamondPath(const QRectF &r) {
  QPainterPath path;
  path.moveTo(r.center().x(), r.top());
  path.lineTo(r.right(), r.center().y());
  path.lineTo(r.center().x(), r.bottom());
  path.lineTo(r.left(), r.center().y());
  path.closeSubpath();
  return path;
}

//! Icona quadrata di \p size px (device pixel ratio \p dpr) col diamante
//! disegnato a codice. Serve al KeyframeNavigator: i .qss dei temi forzano
//! `image: url(transparent.svg)` sui bottoni chiave, quindi le icone su file
//! non si vedrebbero comunque — dipingerle qui e' anche l'unico modo di avere
//! sei stati invece di tre.
inline QPixmap keyDiamondPixmap(const KeyDiamond &d, int size, qreal dpr,
                                const QColor &outline = QColor(0, 0, 0)) {
  QPixmap pm(qRound(size * dpr), qRound(size * dpr));
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  // Un pixel di margine per non tagliare il contorno.
  const QPainterPath path =
      keyDiamondPath(QRectF(1.0, 1.0, size - 2.0, size - 2.0));
  fillKeyRegions(p, path, d);
  if (outline.isValid()) {
    p.setPen(QPen(outline, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  }
  return pm;
}

}  // namespace ZtoryTheme
