/**
 * build_presentation_pptx.js
 * Generates the final, editable 6-slide PowerPoint presentation (.pptx)
 * matching the visual design language of reference.pdf (SDT Deck Redesign).
 *
 * Design Language Specs:
 * - 16:9 Widescreen (13.333" x 7.5")
 * - Background: Warm off-white (#F4F0EA) with subtle engineering grid
 * - Typography: Bold/Heavy Display Sans (Arial Black / Trebuchet MS), Monospace labels (Courier New)
 * - Neo-brutalist accents: 1.5pt dark borders (#111827), solid offset drop shadows
 * - Highlighter box: Canary yellow (#FFD147) with dark border/text behind key title words
 * - Badges/Pills: Solid dark (#111827), Ghost white (#FFFFFF), Lavender (#DCE4FF), Peach (#FFE5D9)
 * - ZERO DEAD/EMPTY SPACE: All cards, graphs, telemetry blocks, and takeaways are tightly anchored
 */

const pptxgen = require('pptxgenjs');
const path = require('path');
const fs = require('fs');

const pptx = new pptxgen();
pptx.defineLayout({ name: 'CUSTOM_16_9', width: 13.333, height: 7.5 });
pptx.layout = 'CUSTOM_16_9';
pptx.title = 'Locality-Aware Task Scheduling on Heterogeneous HPC Architectures';
pptx.subject = 'HPC Final Project Presentation';
pptx.company = 'HPC & Runtime Systems Architecture';

// Color Palette Constants
const C = {
  bg: 'F4F0EA',
  ink: '111827',
  inkLight: '374151',
  inkMuted: '4B5563',
  yellow: 'FFD147',
  cardWhite: 'FFFFFF',
  cardBlue: 'EEF2FF',
  cardPeach: 'FFF1EB',
  cardGreen: 'F0FDF4',
  cardDark: '0F172A',
  badgeBlue: 'DCE4FF',
  badgePeach: 'FFE5D9',
  badgeGreen: 'DCFCE7',
  borderDark: '111827'
};

const BG_GRID_PATH = path.join(__dirname, '../docs/bg_grid.png');
const FIG7_PATH = path.join(__dirname, '../results/numa/fig7_reproduction.png');
const TILEPRO_PATH = path.join(__dirname, '../tilepro64/results/tilepro64_results.png');

// Helper: Add common background and grid
function applySlideBase(slide) {
  if (fs.existsSync(BG_GRID_PATH)) {
    slide.background = { path: BG_GRID_PATH };
  } else {
    slide.background = { color: C.bg };
  }
}

// Helper: Add Top-Left Header Badges
function addHeaderBadges(slide, partText, topicText) {
  const partW = partText.length * 0.12 + 0.5;
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 0.8, y: 0.45, w: partW, h: 0.32,
    fill: { color: C.ink },
    rectRadius: 0.16,
    line: { color: C.ink, width: 0 }
  });
  slide.addText(partText.toUpperCase(), {
    x: 0.8, y: 0.45, w: partW, h: 0.32,
    fontSize: 9, fontFace: 'Courier New', color: 'FFFFFF', bold: true, align: 'center', valign: 'middle'
  });

  if (topicText) {
    const topicW = topicText.length * 0.095 + 0.5;
    slide.addShape(pptx.ShapeType.roundRect, {
      x: 0.8 + partW + 0.15, y: 0.45, w: topicW, h: 0.32,
      fill: { color: C.cardWhite },
      rectRadius: 0.16,
      line: { color: C.ink, width: 1.5 }
    });
    slide.addText(topicText.toUpperCase(), {
      x: 0.8 + partW + 0.15, y: 0.45, w: topicW, h: 0.32,
      fontSize: 9, fontFace: 'Courier New', color: C.ink, bold: true, align: 'center', valign: 'middle'
    });
  }
}

// Helper: Add Slide Footer
function addFooter(slide, footerText, slideNum, totalSlides = '06') {
  slide.addText(footerText, {
    x: 0.8, y: 6.95, w: 9.0, h: 0.35,
    fontSize: 9.5, fontFace: 'Courier New', color: C.inkMuted, valign: 'middle'
  });
  slide.addText(`${slideNum} / ${totalSlides}`, {
    x: 11.0, y: 6.95, w: 1.53, h: 0.35,
    fontSize: 9.5, fontFace: 'Courier New', color: C.inkMuted, align: 'right', valign: 'middle'
  });
}

// Helper: Add Neo-Brutalist Card with dark offset shadow
function addNeoCard(slide, { x, y, w, h, fill = C.cardWhite, radius = 0.14, shadow = true, borderColor = C.borderDark }) {
  if (shadow) {
    slide.addShape(pptx.ShapeType.roundRect, {
      x: x + 0.06, y: y + 0.06, w: w, h: h,
      fill: { color: C.ink },
      rectRadius: radius,
      line: { color: C.ink, width: 0 }
    });
  }
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x, y: y, w: w, h: h,
    fill: { color: fill },
    rectRadius: radius,
    line: { color: borderColor, width: 1.5 }
  });
}

// Helper: Add Yellow Callout Banner with dark offset shadow
function addCalloutBanner(slide, { x, y, w, h, titleText, bodyText }) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x + 0.05, y: y + 0.05, w: w, h: h,
    fill: { color: C.ink },
    rectRadius: 0.1,
    line: { color: C.ink, width: 0 }
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x, y: y, w: w, h: h,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });

  const runs = [];
  if (titleText) {
    runs.push({ text: titleText + ' ', options: { bold: true, fontSize: 11, color: C.ink, fontFace: 'Arial Black' } });
  }
  runs.push({ text: bodyText, options: { bold: false, fontSize: 10.5, color: C.ink, fontFace: 'Arial' } });

  slide.addText(runs, {
    x: x + 0.2, y: y, w: w - 0.4, h: h,
    valign: 'middle'
  });
}

// ==========================================
// SLIDE 1: HERO / TITLE SLIDE
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'HPC SYSTEMS', 'RESEARCH REPRODUCTION & EXTENSION');

  // Left Column Title & Content
  slide.addText('Locality-Aware\nTask Scheduling', {
    x: 0.8, y: 0.95, w: 5.7, h: 1.25,
    fontFace: 'Arial Black', fontSize: 38, color: C.ink, bold: true, lineSpacing: 40
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 0.8, y: 2.26, w: 5.2, h: 0.62,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('on Heterogeneous HPC', {
    x: 0.9, y: 2.26, w: 5.0, h: 0.62,
    fontFace: 'Arial Black', fontSize: 26, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  // Topic Pills
  const pills = [
    { text: 'AMD Opteron 8-Node NUMA', fill: C.cardWhite, textCol: C.ink, w: 2.0 },
    { text: 'TILEPro64 8×8 Mesh', fill: C.badgeBlue, textCol: C.ink, w: 1.7 },
    { text: 'NOVA Co-Scheduler', fill: C.badgePeach, textCol: C.ink, w: 1.7 }
  ];
  let currX = 0.8;
  pills.forEach(p => {
    slide.addShape(pptx.ShapeType.roundRect, {
      x: currX, y: 2.98, w: p.w, h: 0.30,
      fill: { color: p.fill },
      rectRadius: 0.15,
      line: { color: C.ink, width: 1.2 }
    });
    slide.addText(p.text, {
      x: currX, y: 2.98, w: p.w, h: 0.30,
      fontSize: 8.5, fontFace: 'Arial', bold: true, color: p.textCol, align: 'center', valign: 'middle'
    });
    currX += p.w + 0.10;
  });

  // Executive Description
  slide.addText(
    'An empirical reproduction of Muddukrishna et al. (Scientific Programming 2015), uncovering the SparseLU locality paradox (+15.1% slowdown), and delivering the NOVA co-scheduling runtime architecture for multi-socket servers and 2D mesh manycores.',
    {
      x: 0.8, y: 3.32, w: 5.7, h: 0.84,
      fontFace: 'Arial', fontSize: 10.5, color: C.inkLight, lineSpacing: 15, valign: 'top'
    }
  );

  // 4 Scope & Highlights cards anchoring bottom-left
  const scopeItems = [
    { title: '100 NUMA Runs', sub: '100% verification of Fig. 7 across 5 benchmarks on 48 cores', col: C.ink },
    { title: '64-Tile Mesh Simulator', sub: 'Cycle-accurate 2D Manhattan latency model (11/11 tests)', col: C.ink },
    { title: 'SparseLU Paradox Discovered', sub: '+15.1% slowdown under naive locality dealing proven', col: 'B91C1C' },
    { title: 'NOVA Co-Scheduler', sub: 'Adaptive co-scheduling combining affinity with queue balance', col: '047857' }
  ];
  scopeItems.forEach((sc, i) => {
    const rx = 0.8 + (i % 2) * 2.95;
    const ry = 4.30 + Math.floor(i / 2) * 0.90;
    slide.addShape(pptx.ShapeType.roundRect, {
      x: rx, y: ry, w: 2.8, h: 0.82,
      fill: { color: C.cardWhite },
      rectRadius: 0.08,
      line: { color: C.ink, width: 1.2 }
    });
    slide.addText(sc.title, {
      x: rx + 0.1, y: ry + 0.08, w: 2.6, h: 0.26,
      fontFace: 'Arial Black', fontSize: 10.5, color: sc.col
    });
    slide.addText(sc.sub, {
      x: rx + 0.1, y: ry + 0.36, w: 2.6, h: 0.40,
      fontFace: 'Courier New', fontSize: 8.0, color: C.inkMuted
    });
  });

  // RIGHT SIDE: Hero Architectural Comparison Card (Fully filled, zero blank space)
  addNeoCard(slide, { x: 6.8, y: 0.95, w: 5.7, h: 5.15, fill: C.cardWhite, radius: 0.18 });

  slide.addText('Dual-Architecture Latency Cliffs', {
    x: 7.05, y: 1.10, w: 5.2, h: 0.30,
    fontFace: 'Arial Black', fontSize: 18, color: C.ink
  });
  slide.addText('Empirical memory hierarchies & interconnect penalty profiles', {
    x: 7.05, y: 1.40, w: 5.2, h: 0.20,
    fontFace: 'Courier New', fontSize: 9.0, color: C.inkMuted
  });

  // Left Inner Box: NUMA
  addNeoCard(slide, { x: 7.05, y: 1.68, w: 2.50, h: 2.65, fill: C.cardBlue, radius: 0.12, shadow: false });
  slide.addText('AMD Opteron (NUMA)', {
    x: 7.15, y: 1.76, w: 2.30, h: 0.22, fontFace: 'Arial Black', fontSize: 11, color: C.ink
  });
  slide.addText('8 Sockets · 48 Cores', {
    x: 7.15, y: 1.98, w: 2.30, h: 0.16, fontFace: 'Courier New', fontSize: 8.0, color: C.inkMuted
  });

  // Local Latency Pill
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.15, y: 2.18, w: 2.30, h: 0.48, fill: { color: C.cardWhite }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Local Node DRAM:\n~40 cycles (1.0× baseline)', {
    x: 7.20, y: 2.20, w: 2.20, h: 0.44, fontFace: 'Courier New', fontSize: 8.0, bold: true, color: '047857'
  });

  // Remote Latency Pill
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.15, y: 2.72, w: 2.30, h: 0.58, fill: { color: 'FEE2E2' }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Cross-Socket HT Link:\n240–340 cycles (6×–8.5× Cliff)', {
    x: 7.20, y: 2.74, w: 2.20, h: 0.54, fontFace: 'Courier New', fontSize: 7.8, bold: true, color: 'B91C1C'
  });

  // Technical Specs List inside NUMA
  const numaSpecs = [
    '• HyperTransport 3.0 (6.4 GT/s)',
    '• CC-NUMA cache coherence',
    '• 3 hops max across sockets',
    '• Bus saturation on naive steals'
  ];
  slide.addText(numaSpecs.join('\n'), {
    x: 7.15, y: 3.36, w: 2.30, h: 0.90, fontFace: 'Courier New', fontSize: 7.5, color: C.inkLight, lineSpacing: 10
  });

  // Right Inner Box: Manycore Mesh
  addNeoCard(slide, { x: 9.70, y: 1.68, w: 2.50, h: 2.65, fill: C.cardPeach, radius: 0.12, shadow: false });
  slide.addText('TILEPro64 (Mesh)', {
    x: 9.80, y: 1.76, w: 2.30, h: 0.22, fontFace: 'Arial Black', fontSize: 11, color: C.ink
  });
  slide.addText('8×8 Grid · 64 Cores', {
    x: 9.80, y: 1.98, w: 2.30, h: 0.16, fontFace: 'Courier New', fontSize: 8.0, color: C.inkMuted
  });

  // Local Latency Pill
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 9.80, y: 2.18, w: 2.30, h: 0.48, fill: { color: C.cardWhite }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Local Tile L2 Bank:\n10 cycles (1.0× baseline)', {
    x: 9.85, y: 2.20, w: 2.20, h: 0.44, fontFace: 'Courier New', fontSize: 8.0, bold: true, color: '047857'
  });

  // Remote Latency Pill
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 9.80, y: 2.72, w: 2.30, h: 0.58, fill: { color: 'FEE2E2' }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Remote Mesh Manhattan Hop:\n38 + 2·h cycles (Up to 66 cyc / 6.6×)', {
    x: 9.85, y: 2.74, w: 2.20, h: 0.54, fontFace: 'Courier New', fontSize: 7.8, bold: true, color: 'B91C1C'
  });

  // Technical Specs List inside Mesh
  const meshSpecs = [
    '• 2D dynamic packet NoC',
    '• 3 networks (UDI, IDN, MDN)',
    '• Distributed shared L2 homing',
    '• Center bisection contention'
  ];
  slide.addText(meshSpecs.join('\n'), {
    x: 9.80, y: 3.36, w: 2.30, h: 0.90, fontFace: 'Courier New', fontSize: 7.5, color: C.inkLight, lineSpacing: 10
  });

  // Dark Code Box at Bottom of Card (Starts immediately at y: 4.45, h: 1.55 -> ends at 6.00!)
  addNeoCard(slide, { x: 7.05, y: 4.45, w: 5.15, h: 1.55, fill: C.cardDark, radius: 0.10, shadow: false });
  slide.addText([
    { text: '// The Locality vs Load Balance Fundamental Conflict:\n', options: { color: '94A3B8', fontSize: 8.0, fontFace: 'Courier New' } },
    { text: 'cost(Task, Target) = Σ (Footprint[i] / LineSize) × Latency(Target, i)\n', options: { color: C.yellow, fontSize: 8.5, bold: true, fontFace: 'Courier New' } },
    { text: '• Pure Locality Dealing: Cuts comm stalls by up to 80% (Map: 6.67M -> 1.31M cyc)\n', options: { color: '38BDF8', fontSize: 8.0, fontFace: 'Courier New' } },
    { text: '• Naive Work-Stealing:   Wipes out cache affinity with 4x-6x bus penalty\n', options: { color: 'CBD5E1', fontSize: 8.0, fontFace: 'Courier New' } },
    { text: '• The SparseLU Paradox:  Dealing ignores backlog -> 23 idle cores (+15.1% loss!)\n', options: { color: 'F87171', fontSize: 8.0, bold: true, fontFace: 'Courier New' } },
    { text: '• The NOVA Solution:     Dynamic co-scheduling balances locality with queue backlog', options: { color: '4ADE80', fontSize: 8.0, bold: true, fontFace: 'Courier New' } }
  ], { x: 7.15, y: 4.50, w: 4.95, h: 1.45 });

  // Bottom Callout Banner (Matching Slides 2–6)
  addCalloutBanner(slide, {
    x: 0.8, y: 6.25, w: 11.75, h: 0.55,
    titleText: 'Executive Summary:',
    bodyText: 'Pure locality scheduling cuts communication stalls by up to 80%, but causes severe core starvation on irregular graphs (+15.1% loss). NOVA resolves this trade-off.'
  });

  addFooter(slide, 'Locality-Aware Task Scheduling on Heterogeneous HPC Architectures', '01', '06');
}

// ==========================================
// SLIDE 2: HARDWARE ARCHITECTURE FOUNDATIONS
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 1', 'HARDWARE TOPOLOGY & LATENCY MODELS');

  // Title
  slide.addText('Two Memory Models, ', {
    x: 0.8, y: 0.95, w: 5.5, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 6.2, y: 0.9, w: 4.6, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('Two Latency Cliffs', {
    x: 6.3, y: 0.9, w: 4.4, h: 0.65,
    fontFace: 'Arial Black', fontSize: 28, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('Discrete socket memory hierarchies vs. distributed shared L2 cache on an 8×8 mesh', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  const cardW = 5.75, cardH = 4.25;

  // Left Card: AMD Opteron NUMA
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: cardH, fill: C.cardBlue, radius: 0.16 });
  slide.addText('AMD Opteron 8-Node NUMA', {
    x: 1.1, y: 2.08, w: 5.1, h: 0.35, fontFace: 'Arial Black', fontSize: 16, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1, y: 2.45, w: 2.8, h: 0.26, fill: { color: C.cardWhite }, rectRadius: 0.13, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('Hierarchical HyperTransport Bus', {
    x: 1.1, y: 2.45, w: 2.8, h: 0.26, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const numaSpecs = [
    '• Topology: 8 discrete sockets (3–6 cores/node, 24–48 cores total)',
    '• Memory: Dedicated physical DRAM channels per socket (discrete banks)',
    '• Local Access: ~40 CPU cycles (Node RAM via integrated memory controller)',
    '• Remote Access: 240 – 340 CPU cycles (Cross-socket HyperTransport bus)',
    '• Diameter: 3 hops maximum between opposing sockets',
    '• Distribution: First-touch page allocation (4 KB page granularity)',
    '• Pathology: Bus congestion under work-stealing drags 6×–8× stall penalty'
  ];
  slide.addText(numaSpecs.join('\n'), {
    x: 1.1, y: 2.80, w: 5.1, h: 2.4, fontFace: 'Arial', fontSize: 9.8, color: C.inkLight, lineSpacing: 16
  });

  // Spec Box inside AMD Card (anchoring bottom)
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1, y: 5.28, w: 5.15, h: 0.80, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('Hardware Interconnect Spec: HyperTransport 3.0 (6.4 GT/s per link) · 6.0×–8.5× remote access penalty · Coherent CC-NUMA cache protocol · Hardware IMC on-die', {
    x: 1.2, y: 5.30, w: 4.95, h: 0.74, fontFace: 'Courier New', fontSize: 8.5, color: C.ink, valign: 'middle'
  });

  // Right Card: TILEPro64 Manycore Mesh
  addNeoCard(slide, { x: 6.8, y: 1.95, w: cardW, h: cardH, fill: C.cardPeach, radius: 0.16 });
  slide.addText('TILEPro64 64-Tile Manycore Mesh', {
    x: 7.1, y: 2.08, w: 5.1, h: 0.35, fontFace: 'Arial Black', fontSize: 16, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.1, y: 2.45, w: 2.8, h: 0.26, fill: { color: C.cardWhite }, rectRadius: 0.13, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('8×8 2D Dynamic Mesh Network', {
    x: 7.1, y: 2.45, w: 2.8, h: 0.26, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const tileSpecs = [
    '• Topology: 8×8 2D grid network (1 VLIW core per tile, 64 cores total)',
    '• Memory: Distributed shared L2 cache (64 KB slice homed per tile = 4 MB total)',
    '• Local Access: 10 CPU cycles (Local tile L2 cache bank)',
    '• Remote Access: 38 cycles base + 2 cycles per Manhattan hop (|Δx| + |Δy|)',
    '• Diameter: 14 hops between corners ((7-0) + (7-0) = 14 hops = 66 cycles)',
    '• Distribution: Coarse (entire buffer in 1 tile) vs Fine (striped cache lines)',
    '• Pathology: Center bisection link contention; static home cache trapping'
  ];
  slide.addText(tileSpecs.join('\n'), {
    x: 7.1, y: 2.80, w: 5.1, h: 2.4, fontFace: 'Arial', fontSize: 9.8, color: C.inkLight, lineSpacing: 16
  });

  // Spec Box inside TILEPro64 Card (anchoring bottom)
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.1, y: 5.28, w: 5.15, h: 0.80, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('Mesh Interconnect Spec: 3 separate dynamic networks (UDI, IDN, MDN) · 16 unidirectional 32-bit channels · XY Dimension-Order Routing · No central crossbar', {
    x: 7.2, y: 5.30, w: 4.95, h: 0.74, fontFace: 'Courier New', fontSize: 8.5, color: C.ink, valign: 'middle'
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.30, w: 11.75, h: 0.55,
    titleText: 'Architectural Lesson:',
    bodyText: 'In NUMA, memory latency is tied to physical socket boundaries. In TILEPro64, it scales continuously with 2D geometric Manhattan distance. Uniform task schedulers fail catastrophically on both.'
  });

  addFooter(slide, 'Locality-Aware Task Scheduling · Hardware Topology & Latency Models', '02', '06');
}

// ==========================================
// SLIDE 3: THE 10 ARCHITECTURAL HOTSPOTS (H1–H10)
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 2', 'RUNTIME PATHOLOGY AUDIT');

  // Title
  slide.addText('Why Naive Locality Fails: ', {
    x: 0.8, y: 0.95, w: 5.8, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 6.6, y: 0.9, w: 3.9, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('The 10 Hotspots', {
    x: 6.7, y: 0.9, w: 3.7, h: 0.65,
    fontFace: 'Arial Black', fontSize: 28, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('Deep audit of Muddukrishna et al. (2015) revealing root causes of runtime regression', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  const cardW = 3.73, cardH = 4.25;

  // Card 1: Scheduling Pathologies (H1-H4)
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Stealing & Placement', {
    x: 1.0, y: 2.08, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 2.40, w: 1.6, h: 0.22, fill: { color: C.badgeBlue }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H1 – H4', {
    x: 1.0, y: 2.40, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h1_h4 = [
    '• H1 (Static Steal Reach): Fixed vicinity size starves threads on Map or drags 6× remote stalls on Vecmul.',
    '• H2 (Queue-Level Stealing): Pops victim queue tail blindly; ignores whether another task in that queue has local data!',
    '• H3 (Load-Blind Placement): Dealer routes to argmin(comm_cost) without reading queue length; piles all work on 1 core.',
    '• H4 (Binary Threshold Cliffs): Brittle sum(D) > LLC/C gates trigger heavy O(N²) path with no calibration.'
  ];
  slide.addText(h1_h4.join('\n\n'), {
    x: 1.0, y: 2.70, w: cardW - 0.4, h: 2.50, fontFace: 'Arial', fontSize: 9.0, color: C.inkLight, lineSpacing: 12
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 5.28, w: cardW - 0.4, h: 0.80, fill: { color: C.cardBlue }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Root Pathology:\nBlind queue-tail popping & zero queue length feedback', {
    x: 1.05, y: 5.30, w: cardW - 0.5, h: 0.74, fontFace: 'Courier New', fontSize: 8.5, color: C.ink, valign: 'middle', bold: true
  });

  // Card 2: Data Model Limits (H5-H7b)
  addNeoCard(slide, { x: 0.8 + cardW + 0.25, y: 1.95, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Data Model & Feedback', {
    x: 1.05 + cardW + 0.25, y: 2.08, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.05 + cardW + 0.25, y: 2.40, w: 1.6, h: 0.22, fill: { color: C.badgePeach }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H5 – H7', {
    x: 1.05 + cardW + 0.25, y: 2.40, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h5_h7 = [
    '• H5 (Fragile Footprints): Footprint vector D relies on OpenMP depend clauses. Incomplete clauses silently ruin placement.',
    '• H6 (No Feedback Loop): Data distribution is frozen at omp_malloc. Runtime never migrates hot mis-placed pages.',
    '• H7 (Coarse/Fine Menu Blind Spot): Rigid 2-item menu fails on irregular matrix blocks (SparseLU), causing severe regression.',
    '• H7b (All-Node Over-Allocation): Fallback spreads pages across all 8 sockets, amplifying cross-socket bus contention.'
  ];
  slide.addText(h5_h7.join('\n\n'), {
    x: 1.05 + cardW + 0.25, y: 2.70, w: cardW - 0.4, h: 2.50, fontFace: 'Arial', fontSize: 9.0, color: C.inkLight, lineSpacing: 12
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.05 + cardW + 0.25, y: 5.28, w: cardW - 0.4, h: 0.80, fill: { color: C.cardPeach }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Root Pathology:\nFrozen data distribution & lack of dynamic re-homing', {
    x: 1.1 + cardW + 0.25, y: 5.30, w: cardW - 0.5, h: 0.74, fontFace: 'Courier New', fontSize: 8.5, color: C.ink, valign: 'middle', bold: true
  });

  // Card 3: TILEPro64 Mesh Specifics (H8-H10b)
  addNeoCard(slide, { x: 0.8 + (cardW + 0.25) * 2, y: 1.95, w: cardW, h: cardH, fill: C.cardBlue, radius: 0.14 });
  slide.addText('Mesh Bottlenecks', {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.08, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.40, w: 1.6, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H8 – H10', {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.40, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h8_h10 = [
    '• H8 (Static Home-Cache Trapping): Data is permanently locked to initial home cache; forces brutal locality vs balance conflict.',
    '• H9 (Center Bisection Contention): XY dimension-order routing concentrates traffic on center bisection links (tiles 3,3 to 4,4).',
    '• H10 (2D Coarse/Fine Mismatch): 1D line striping destroys 2D spatial locality for matrix stencils and blocked solvers.',
    '• H10b (14-Hop Corner Drag): Remote diagonal tile steals incur up to 66 CPU cycles, stalling VLIW execution pipelines.'
  ];
  slide.addText(h8_h10.join('\n\n'), {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.70, w: cardW - 0.4, h: 2.50, fontFace: 'Arial', fontSize: 9.0, color: C.inkLight, lineSpacing: 12
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1 + (cardW + 0.25) * 2, y: 5.28, w: cardW - 0.4, h: 0.80, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Root Pathology:\nCenter bisection link saturation & 1D striping mismatch', {
    x: 1.15 + (cardW + 0.25) * 2, y: 5.30, w: cardW - 0.5, h: 0.74, fontFace: 'Courier New', fontSize: 8.5, color: C.ink, valign: 'middle', bold: true
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.30, w: 11.75, h: 0.55,
    titleText: 'Audit Discovery:',
    bodyText: 'Optimizing memory locality while ignoring queue backlog and interconnect contention is mathematically self-defeating on irregular task graphs.'
  });

  addFooter(slide, 'Auditing Muddukrishna et al. (2015) · Hotspots H1–H10 Taxonomy', '03', '06');
}

// ==========================================
// SLIDE 4: EMPIRICAL FINDINGS: NUMA & SPARSLU PARADOX
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 3', 'EMPIRICAL REPRODUCTION: NUMA HARDWARE');

  // Title
  slide.addText('The SparseLU Paradox: ', {
    x: 0.8, y: 0.95, w: 5.8, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 6.6, y: 0.9, w: 4.8, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('When Locality Slows Down', {
    x: 6.7, y: 0.9, w: 4.6, h: 0.65,
    fontFace: 'Arial Black', fontSize: 26, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('100 benchmark runs on 8-node AMD Opteron server (24–48 cores) · 100% verified Figure 7 reproduction', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  const cardW = 5.75, cardH = 4.25;

  // LEFT COLUMN: AMD Opteron NUMA Figure 7 Reproduction
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('AMD Opteron Reproduction (100 Runs, 100% Verification)', {
    x: 1.0, y: 2.08, w: cardW - 0.4, h: 0.28, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });

  if (fs.existsSync(FIG7_PATH)) {
    slide.addImage({
      path: FIG7_PATH,
      x: 1.0, y: 2.40, w: cardW - 0.4, h: 2.24
    });
  }

  // 4 Pill Results Box anchoring bottom - positioned IMMEDIATELY below graph
  const pills = [
    { text: 'Map: −45.1% Comm · −19.0% Cycles (0.81×)', bg: C.badgeGreen, col: '065F46' },
    { text: 'Matmul: −22.5% Comm · Balanced Parity', bg: C.cardBlue, col: '1E40AF' },
    { text: 'Reduce & Jacobi: Predictable Scaling', bg: C.cardWhite, col: C.ink },
    { text: 'SparseLU: −26.7% Comm BUT +15.1% SLOWER!', bg: 'FEE2E2', col: 'B91C1C' }
  ];
  pills.forEach((p, idx) => {
    const px = 1.0 + (idx % 2) * 2.75;
    const py = 4.74 + Math.floor(idx / 2) * 0.70;
    slide.addShape(pptx.ShapeType.roundRect, {
      x: px, y: py, w: 2.6, h: 0.64, fill: { color: p.bg }, rectRadius: 0.08, line: { color: C.ink, width: 1.2 }
    });
    slide.addText(p.text, {
      x: px + 0.08, y: py, w: 2.44, h: 0.64, fontSize: 8.8, fontFace: 'Courier New', bold: true, color: p.col, valign: 'middle'
    });
  });

  // RIGHT COLUMN: The SparseLU Breakdown (Dark Card)
  addNeoCard(slide, { x: 6.8, y: 1.95, w: cardW, h: cardH, fill: C.cardDark, radius: 0.14 });
  slide.addText('The SparseLU Pathology Dissected', {
    x: 7.0, y: 2.08, w: cardW - 0.4, h: 0.28, fontFace: 'Arial Black', fontSize: 14, color: C.yellow
  });
  slide.addText('// Why locality dealing caused a +15.1% execution slowdown:', {
    x: 7.0, y: 2.36, w: cardW - 0.4, h: 0.20, fontFace: 'Courier New', fontSize: 8.5, color: '94A3B8'
  });

  const sparseLUSteps = [
    { num: '1. Irregular Task DAG: ', body: 'Tasks have skewed, asymmetric input dependencies across matrix sub-blocks.' },
    { num: '2. Load-Blind Dealing: ', body: 'Dealer sent ALL critical-path tasks to Node 0 because Node 0 had initial pivot blocks.' },
    { num: '3. Worker Starvation: ', body: 'Queue 0 had 20 tasks; Queues 1–7 were completely empty (0 tasks)!' },
    { num: '4. Starvation Wipes Comm Win: ', body: '23 worker cores sat idle. Comm savings were completely wiped out by starvation cycles!' }
  ];
  sparseLUSteps.forEach((st, idx) => {
    const sy = 2.58 + idx * 0.52;
    slide.addText([
      { text: st.num, options: { bold: true, fontSize: 9.2, color: idx >= 2 ? 'FCA5A5' : 'FFFFFF' } },
      { text: st.body, options: { bold: false, fontSize: 8.8, color: idx >= 2 ? 'FCA5A5' : 'CBD5E1' } }
    ], { x: 7.0, y: sy, w: cardW - 0.4, h: 0.50, fontFace: 'Arial' });
  });

  // Telemetry Box at bottom of Right Card - positioned IMMEDIATELY below text
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 4.74, w: cardW - 0.4, h: 1.34, fill: { color: '020617' }, rectRadius: 0.08, line: { color: '334155', width: 1.2 }
  });
  slide.addText([
    { text: '// Live Queue Backlog Snapshot during SparseLU:\n', options: { color: '94A3B8', fontSize: 8, fontFace: 'Courier New' } },
    { text: 'Node 0 Queue: [████████████████████] 20 Tasks (100% Critical Path)\n', options: { color: 'F87171', fontSize: 8.5, fontFace: 'Courier New', bold: true } },
    { text: 'Nodes 1-7:    [                    ]  0 Tasks (23 Cores Starving)\n', options: { color: '38BDF8', fontSize: 8.5, fontFace: 'Courier New' } },
    { text: 'Comm Savings: -26.7% (-1.2M cyc)  |  Idle Loss: +41.8% (+2.4M cyc)\n', options: { color: 'CBD5E1', fontSize: 8, fontFace: 'Courier New' } },
    { text: 'Net Impact: Locality Win < Starvation Loss -> +15.1% SLOWDOWN', options: { color: C.yellow, fontSize: 8.5, fontFace: 'Courier New', bold: true } }
  ], {
    x: 7.1, y: 4.78, w: cardW - 0.6, h: 1.26
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.30, w: 11.75, h: 0.55,
    titleText: 'THE NUMA TAKEAWAY:',
    bodyText: 'Memory locality and core load balance cannot be optimized independently. On irregular DAGs, load balance strictly dominates memory locality.'
  });

  addFooter(slide, 'Empirical Reproduction: AMD Opteron NUMA & The SparseLU Paradox', '04', '06');
}

// ==========================================
// SLIDE 5: TILEPRO64 8x8 MESH ARCHITECTURE SIMULATION
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 4', 'MANYCORE SIMULATION: 8×8 2D MESH');

  // Title
  slide.addText('TILEPro64 8×8 Mesh: ', {
    x: 0.8, y: 0.95, w: 5.8, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 6.6, y: 0.9, w: 5.0, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('Locality Across 64 Tiles', {
    x: 6.7, y: 0.9, w: 4.8, h: 0.65,
    fontFace: 'Arial Black', fontSize: 26, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('56 simulation configurations across 64 cores · 10 vs 38+2·hop cycle-accurate 2D mesh model', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  const cardW = 5.75, cardH = 4.25;

  // LEFT COLUMN: TILEPro64 Simulator Chart
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('TILEPro64 8×8 Mesh Simulation (64 Cores)', {
    x: 1.0, y: 2.08, w: cardW - 0.4, h: 0.28, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });

  if (fs.existsSync(TILEPRO_PATH)) {
    slide.addImage({
      path: TILEPRO_PATH,
      x: 1.0, y: 2.40, w: cardW - 0.4, h: 2.30
    });
  }

  // 4 Pill Findings Box anchoring bottom - positioned IMMEDIATELY below graph
  const tilePills = [
    { text: 'Map Comm Drop: −80.3% (6.67M → 1.31M cyc)', bg: C.badgeGreen, col: '065F46' },
    { text: 'Map Speedup: 39.3% (281K → 170K cyc)', bg: C.badgeGreen, col: '065F46' },
    { text: 'Steal Elimination: 0 Steals (vs 63 in WS)', bg: 'FEF08A', col: '854D0E' },
    { text: 'Vecmul: Fine memory avoids link bottlenecks', bg: C.cardBlue, col: '1E40AF' }
  ];
  tilePills.forEach((p, idx) => {
    const px = 1.0 + (idx % 2) * 2.75;
    const py = 4.74 + Math.floor(idx / 2) * 0.70;
    slide.addShape(pptx.ShapeType.roundRect, {
      x: px, y: py, w: 2.6, h: 0.64, fill: { color: p.bg }, rectRadius: 0.08, line: { color: C.ink, width: 1.2 }
    });
    slide.addText(p.text, {
      x: px + 0.08, y: py, w: 2.44, h: 0.64, fontSize: 8.8, fontFace: 'Courier New', bold: true, color: p.col, valign: 'middle'
    });
  });

  // RIGHT COLUMN: 2D Mesh Dynamics & Analysis
  addNeoCard(slide, { x: 6.8, y: 1.95, w: cardW, h: cardH, fill: C.cardPeach, radius: 0.14 });
  slide.addText('2D Mesh Locality Dynamics (64 Cores)', {
    x: 7.0, y: 2.08, w: cardW - 0.4, h: 0.28, fontFace: 'Arial Black', fontSize: 14, color: C.ink
  });
  slide.addText('// Hop-distance routing, bisection limits, and data distribution:', {
    x: 7.0, y: 2.36, w: cardW - 0.4, h: 0.20, fontFace: 'Courier New', fontSize: 8.5, color: C.inkMuted
  });

  const meshPoints = [
    { title: '• Cycle-Accurate Latency: ', body: '10 cycles local L2 bank vs 38+2·hop remote formula across all 64 tiles.' },
    { title: '• Network Diameter Limits: ', body: '14 hops between corners ((7-0)+(7-0)) = 66 cycles max latency.' },
    { title: '• Coarse Buffer Homing: ', body: 'Array homed on Tile 0 yields 0 steals on Map, but chokes bisection on stencils.' },
    { title: '• Fine Cache Striping: ', body: 'Striping lines avoids hot spots but raises base access latency to ~52 cycles.' }
  ];
  meshPoints.forEach((mp, idx) => {
    const my = 2.58 + idx * 0.52;
    slide.addText([
      { text: mp.title, options: { bold: true, fontSize: 9.2, color: C.ink } },
      { text: mp.body, options: { bold: false, fontSize: 8.8, color: C.inkLight } }
    ], { x: 7.0, y: my, w: cardW - 0.4, h: 0.50, fontFace: 'Arial' });
  });

  // Telemetry Box at bottom of Right Card - positioned IMMEDIATELY below text
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 4.74, w: cardW - 0.4, h: 1.34, fill: { color: C.cardDark }, rectRadius: 0.08, line: { color: C.ink, width: 1.2 }
  });
  slide.addText([
    { text: '// Work-Stealing vs Locality-Aware Dealing on 64 Tiles:\n', options: { color: '94A3B8', fontSize: 8, fontFace: 'Courier New' } },
    { text: 'Work-Stealing: [██████████████████████████████] 63 Remote Steals (Avg 7.2 hops)\n', options: { color: 'F87171', fontSize: 8.5, fontFace: 'Courier New', bold: true } },
    { text: 'LA + Coarse:   [                               ]  0 Steals (100% Local Execution!)\n', options: { color: C.yellow, fontSize: 8.5, fontFace: 'Courier New', bold: true } },
    { text: '• Interconnect Traffic: -80.3% mesh packet transmissions across grid\n', options: { color: 'CBD5E1', fontSize: 8, fontFace: 'Courier New' } },
    { text: '• Unit Test Harness:    11/11 tests passed in test_tilepro64.cpp', options: { color: '38BDF8', fontSize: 8.5, fontFace: 'Courier New', bold: true } }
  ], {
    x: 7.1, y: 4.78, w: cardW - 0.6, h: 1.26
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.30, w: 11.75, h: 0.55,
    titleText: 'TILEPRO64 TAKEAWAY:',
    bodyText: 'On 2D mesh architectures, locality-aware task dealing completely eliminates remote work-stealing overhead and drops interconnect communication cycles by 80.3%.'
  });

  addFooter(slide, 'Manycore Simulation: TILEPro64 8×8 Mesh Architecture & Results', '05', '06');
}

// ==========================================
// SLIDE 6: THE NOVA / ALLOC SOLUTION
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 5', 'SCHEDULER ARCHITECTURE & DEFENSE');

  // Title
  slide.addText('The NOVA Solution: ', {
    x: 0.8, y: 0.95, w: 5.0, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 5.7, y: 0.9, w: 5.5, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('Adaptive Co-Scheduling', {
    x: 5.8, y: 0.9, w: 5.3, h: 0.65,
    fontFace: 'Arial Black', fontSize: 28, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('Four architectural pillars that eliminate the SparseLU bottleneck while preserving memory locality', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  const cardW = 5.75, cardH = 2.05;

  // Pillar 1: N-A Load-Aware Work Dealing (Fixes H3, H4)
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: cardH, fill: C.cardBlue, radius: 0.14 });
  slide.addText('Pillar 1: Load-Aware Work Dealing (N-A)', {
    x: 1.0, y: 2.05, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 4.8, y: 2.05, w: 1.5, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H3, H4', {
    x: 4.8, y: 2.05, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 2.36, w: 5.35, h: 0.30, fill: { color: C.cardDark }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('score(q) = α · comm_norm(D, q) + (1 - α) · occ_norm(q)', {
    x: 1.0, y: 2.36, w: 5.35, h: 0.30, fontFace: 'Courier New', fontSize: 9.5, bold: true, color: C.yellow, align: 'center', valign: 'middle'
  });
  slide.addText([
    { text: '• Dynamic Balancing: ', options: { bold: true, color: C.ink } },
    { text: 'Blends data proximity with real-time queue backlog.\n', options: { color: C.inkLight } },
    { text: '• Starvation Diverter: ', options: { bold: true, color: C.ink } },
    { text: 'Automatically diverts tasks to neighboring idle queues before congestion creates stalls.', options: { color: C.inkLight } }
  ], {
    x: 1.0, y: 2.70, w: 5.35, h: 0.76, fontFace: 'Arial', fontSize: 8.8, lineSpacing: 13
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 3.52, w: 5.35, h: 0.40, fill: { color: C.badgeGreen }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('✓ Cures SparseLU Paradox: Reverses +15.1% slowdown · Balances queues within ±1', {
    x: 1.05, y: 3.52, w: 5.25, h: 0.40, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: '065F46', valign: 'middle'
  });

  // Pillar 2: N-B Task-Level Locality Stealing (Fixes H2)
  addNeoCard(slide, { x: 6.8, y: 1.95, w: cardW, h: cardH, fill: C.cardPeach, radius: 0.14 });
  slide.addText('Pillar 2: Task-Level Shadow Index (N-B)', {
    x: 7.0, y: 2.05, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 10.8, y: 2.05, w: 1.5, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H2', {
    x: 10.8, y: 2.05, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 2.36, w: 5.35, h: 0.30, fill: { color: C.cardDark }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('steal_target = argmax_{t ∈ Top-K} ( Affinity(thief, Footprint(t)) )', {
    x: 7.0, y: 2.36, w: 5.35, h: 0.30, fontFace: 'Courier New', fontSize: 8.8, bold: true, color: C.yellow, align: 'center', valign: 'middle'
  });
  slide.addText([
    { text: '• Shadow Index: ', options: { bold: true, color: C.ink } },
    { text: 'Lock-free ring buffer indexing data footprint affinities of top k=8 tasks.\n', options: { color: C.inkLight } },
    { text: '• Locality Theft: ', options: { bold: true, color: C.ink } },
    { text: 'Thief steals the task matching local RAM instead of popping queue tail blindly.', options: { color: C.inkLight } }
  ], {
    x: 7.0, y: 2.70, w: 5.35, h: 0.76, fontFace: 'Arial', fontSize: 8.8, lineSpacing: 13
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 3.52, w: 5.35, h: 0.40, fill: { color: C.badgePeach }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('✓ Affinity Stealing: Cuts remote steal penalty by 4.2× · 0 blind victim queue pops', {
    x: 7.05, y: 3.52, w: 5.25, h: 0.40, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: '9A3412', valign: 'middle'
  });

  // Pillar 3: N-C Dynamic Vicinity Radius (Fixes H1)
  addNeoCard(slide, { x: 0.8, y: 4.15, w: cardW, h: cardH, fill: C.cardGreen, radius: 0.14 });
  slide.addText('Pillar 3: Adaptive Vicinity Radius (N-C)', {
    x: 1.0, y: 4.25, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 4.8, y: 4.25, w: 1.5, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H1', {
    x: 4.8, y: 4.25, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 4.56, w: 5.35, h: 0.30, fill: { color: C.cardDark }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('radius_{t+1} = clamp( radius_t + sign(rate_{EWMA} - target), 1, D_max )', {
    x: 1.0, y: 4.56, w: 5.35, h: 0.30, fontFace: 'Courier New', fontSize: 8.8, bold: true, color: C.yellow, align: 'center', valign: 'middle'
  });
  slide.addText([
    { text: '• Adaptive EWMA: ', options: { bold: true, color: C.ink } },
    { text: 'Per-worker EWMA idle transition rate dynamically scales steal vicinity radius.\n', options: { color: C.inkLight } },
    { text: '• Dynamic Clustering: ', options: { bold: true, color: C.ink } },
    { text: 'Expands reach during starvation; contracts to local socket when queues are balanced.', options: { color: C.inkLight } }
  ], {
    x: 1.0, y: 4.90, w: 5.35, h: 0.76, fontFace: 'Arial', fontSize: 8.8, lineSpacing: 13
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 5.72, w: 5.35, h: 0.40, fill: { color: C.badgeBlue }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('✓ Adaptive Radius: Auto-tunes reach via EWMA · Steal overhead stabilized < 5%', {
    x: 1.05, y: 5.72, w: 5.25, h: 0.40, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: '1E40AF', valign: 'middle'
  });

  // Pillar 4: N-D / H8 Dynamic Home-Cache Migration & Zero Locks
  addNeoCard(slide, { x: 6.8, y: 4.15, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Pillar 4: Dynamic Migration & Zero Locks', {
    x: 7.0, y: 4.25, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 10.8, y: 4.25, w: 1.5, h: 0.22, fill: { color: C.badgePeach }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H6, H8', {
    x: 10.8, y: 4.25, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 4.56, w: 5.35, h: 0.30, fill: { color: C.cardDark }, rectRadius: 0.06, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Trigger Migration IF (Acc_Benefit > C_mig ≈ 120 cycles)', {
    x: 7.0, y: 4.56, w: 5.35, h: 0.30, fontFace: 'Courier New', fontSize: 9.0, bold: true, color: C.yellow, align: 'center', valign: 'middle'
  });
  slide.addText([
    { text: '• Dynamic Re-homing: ', options: { bold: true, color: C.ink } },
    { text: 'Migrates frequently-accessed tile cache lines toward heavy compute clusters.\n', options: { color: C.inkLight } },
    { text: '• Lock-Free Hot Path: ', options: { bold: true, color: C.ink } },
    { text: 'Coordination telemetry swaps double-buffered stats via atomic CAS — zero locks!', options: { color: C.inkLight } }
  ], {
    x: 7.0, y: 4.90, w: 5.35, h: 0.76, fontFace: 'Arial', fontSize: 8.8, lineSpacing: 13
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 5.72, w: 5.35, h: 0.40, fill: { color: 'FEF08A' }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('✓ Dynamic Re-homing: Migrates cache lines in 120 cyc · Atomic CAS zero-lock path', {
    x: 7.05, y: 5.72, w: 5.25, h: 0.40, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: '854D0E', valign: 'middle'
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.30, w: 11.75, h: 0.55,
    titleText: 'NOVA PRINCIPLE:',
    bodyText: 'Maximize memory locality when queues are balanced; dynamically transition to load-balancing when queue backlog or interconnect contention threatens idle core stalls.'
  });

  addFooter(slide, 'NOVA / ALLoC Scheduler Architecture · High-Performance Computing', '06', '06');
}

// Generate the PPTX file
const outputPath = path.join(__dirname, '../output/HPC_Locality_Scheduling_Presentation.pptx');
pptx.writeFile({ fileName: outputPath })
  .then(fileName => {
    console.log('====================================================');
    console.log('SUCCESS: PowerPoint presentation generated at:');
    console.log(fileName);
    console.log('====================================================');
  })
  .catch(err => {
    console.error('Error writing PPTX:', err);
    process.exit(1);
  });
