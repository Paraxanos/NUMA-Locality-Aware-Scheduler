import PptxGenJS from 'pptxgenjs';
import { COLORS, TYPO, FONTS } from './theme';
import { addFooter, addPill, addTitle, addSubtitle } from './components';

export function buildDeck(): PptxGenJS.Presentation {
  const pres = new PptxGenJS();
  pres.defineLayout({ name: 'WIDE', width: 13.333, height: 7.5 });
  pres.layout = 'WIDE';

  // ========== SLIDE 1: TITLE / HERO ==========
  const s1 = pres.addSlide();
  s1.background = { color: COLORS.darkNavy };

  // Thin orange decorative line at top
  s1.addShape(pres.ShapeType.rect, {
    x: 0.55, y: 0.35, w: 1.8, h: 0.06,
    fill: { color: COLORS.orangeAccent }, line: { color: COLORS.orangeAccent, width: 0 },
  });

  // Small label pill
  s1.addShape(pres.ShapeType.roundRect, {
    x: 0.55, y: 0.55, w: 2.6, h: 0.3,
    rectRadius: 0.15, fill: { color: COLORS.navy }, line: { color: COLORS.orangeAccent, width: 1 },
  });
  s1.addText('DISSERTATION · 2026', {
    x: 0.65, y: 0.55, w: 2.4, h: 0.3,
    fontSize: 9, bold: true, color: COLORS.yellowAccent, charSpacing: 3,
  });

  // Main headline — bold, condensed, editorial
  s1.addText('Locality-Aware Scheduling\non Heterogeneous HPC Architectures', {
    x: 0.55, y: 1.25, w: 11.5, h: 2.2,
    fontSize: 48, bold: true, color: COLORS.paper,
    fontFace: FONTS.display, align: 'left', valign: 'top', charSpacing: -3, lineSpacing: 44,
  });

  // Sub-headline
  s1.addText('Reproducing & extending Muddukrishna et al. (2015) — Two architectures, one co-scheduled solution.', {
    x: 0.55, y: 3.6, w: 10, h: 0.7,
    fontSize: 18, color: COLORS.muted, fontFace: FONTS.body, italic: true,
  });

  // Dual architecture cards (asymmetric: left dark card / right navy card)
  // Left: NUMA
  s1.addShape(pres.ShapeType.roundRect, {
    x: 0.55, y: 4.7, w: 5.6, h: 1.9,
    rectRadius: 0.08, fill: { color: '#15223a' }, line: { color: COLORS.blueAccent, width: 1.5 },
  });
  s1.addText('AMERICAN OPTERON 8-NODE NUMA', {
    x: 0.85, y: 4.9, w: 5, h: 0.3, fontSize: 11, bold: true, color: COLORS.yellowAccent, charSpacing: 2,
  });
  s1.addText('HyperTransport interconnect  ·  3–6 cores/node  ·  Local ~40 cycles  ·  Remote 240–340 cycles', {
    x: 0.85, y: 5.2, w: 5, h: 0.4, fontSize: 10, color: COLORS.paper,
  });
  s1.addText('100 benchmark runs  ·  5 kernels  ·  4 configs  ·  5 reps  ·  100% check_ok=1', {
    x: 0.85, y: 5.7, w: 5, h: 0.35, fontSize: 9, color: COLORS.muted,
  });
  s1.addText('15.1% SparseLU slowdown revealed — the paradox.', {
    x: 0.85, y: 6.15, w: 5, h: 0.35, fontSize: 10, color: COLORS.orangeAccent, bold: true,
  });

  // Right: TILEPro64
  s1.addShape(pres.ShapeType.roundRect, {
    x: 6.6, y: 4.7, w: 6.1, h: 1.9,
    rectRadius: 0.08, fill: { color: '#0c1a30' }, line: { color: COLORS.orangeAccent, width: 1.5 },
  });
  s1.addText('TILEPro64 8×8 MESH MANYCORE', {
    x: 6.9, y: 4.9, w: 5.5, h: 0.3, fontSize: 11, bold: true, color: COLORS.yellowAccent, charSpacing: 2,
  });
  s1.addText('Manhattan hop distance  ·  64 tiles  ·  Local = 10 cycles  ·  Remote = 38 + 2·hops', {
    x: 6.9, y: 5.2, w: 5.5, h: 0.4, fontSize: 10, color: COLORS.paper,
  });
  s1.addText('11 validation tests passed  ·  56 experimental runs  ·  80.3% comm cost drop  ·  39.3% speedup', {
    x: 6.9, y: 5.7, w: 5.5, h: 0.35, fontSize: 9, color: COLORS.muted,
  });
  s1.addText('NOVA / ALLoC co-scheduler — zero-contention control plane.', {
    x: 6.9, y: 6.15, w: 5.5, h: 0.35, fontSize: 10, color: COLORS.orangeAccent, bold: true,
  });

  // Footer label
  s1.addText('CHIRAG YESHWANT  ·  HIGH-PERFORMANCE COMPUTING  ·  UNIVERSITY RESEARCH PROJECT 2026', {
    x: 0.55, y: 7.05, w: 12, h: 0.2,
    fontSize: 7, color: COLORS.muted, charSpacing: 1,
  });

  return pres;
}
