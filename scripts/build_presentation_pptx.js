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
 * - Embedded high-res charts and technical code snippets
 */

const pptxgen = require('pptxgenjs');
const path = require('path');
const fs = require('fs');

const pptx = new pptxgen();
pptx.layout = 'LAYOUT_16x9';
pptx.title = 'Locality-Aware Task Scheduling on Heterogeneous HPC Architectures';
pptx.subject = 'HPC Final Project Presentation';
pptx.author = 'Qasim, Awadesh, Devashish, Husaam, Chirag';
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
  // Dark Badge
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 0.8, y: 0.45, w: partText.length * 0.12 + 0.5, h: 0.32,
    fill: { color: C.ink },
    rectRadius: 0.16,
    line: { color: C.ink, width: 0 }
  });
  slide.addText(partText.toUpperCase(), {
    x: 0.8, y: 0.45, w: partText.length * 0.12 + 0.5, h: 0.32,
    fontSize: 9, fontFace: 'Courier New', color: 'FFFFFF', bold: true, align: 'center', valign: 'middle'
  });

  const partW = partText.length * 0.12 + 0.5;
  // White/Ghost Badge
  if (topicText) {
    slide.addShape(pptx.ShapeType.roundRect, {
      x: 0.8 + partW + 0.15, y: 0.45, w: topicText.length * 0.095 + 0.5, h: 0.32,
      fill: { color: C.cardWhite },
      rectRadius: 0.16,
      line: { color: C.ink, width: 1.5 }
    });
    slide.addText(topicText.toUpperCase(), {
      x: 0.8 + partW + 0.15, y: 0.45, w: topicText.length * 0.095 + 0.5, h: 0.32,
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
function addNeoCard(slide, { x, y, w, h, fill = C.cardWhite, radius = 0.15, shadow = true, borderColor = C.borderDark }) {
  if (shadow) {
    // Solid offset shadow
    slide.addShape(pptx.ShapeType.roundRect, {
      x: x + 0.06, y: y + 0.06, w: w, h: h,
      fill: { color: C.ink },
      rectRadius: radius,
      line: { color: C.ink, width: 0 }
    });
  }
  // Front Card
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x, y: y, w: w, h: h,
    fill: { color: fill },
    rectRadius: radius,
    line: { color: borderColor, width: 1.5 }
  });
}

// Helper: Add Yellow Callout Banner with dark offset shadow
function addCalloutBanner(slide, { x, y, w, h, titleText, bodyText }) {
  // Offset shadow
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x + 0.06, y: y + 0.06, w: w, h: h,
    fill: { color: C.ink },
    rectRadius: 0.12,
    line: { color: C.ink, width: 0 }
  });
  // Front yellow box
  slide.addShape(pptx.ShapeType.roundRect, {
    x: x, y: y, w: w, h: h,
    fill: { color: C.yellow },
    rectRadius: 0.12,
    line: { color: C.ink, width: 1.5 }
  });

  const runs = [];
  if (titleText) {
    runs.push({ text: titleText + ' ', options: { bold: true, fontSize: 11.5, color: C.ink, fontFace: 'Arial Black' } });
  }
  runs.push({ text: bodyText, options: { bold: false, fontSize: 11, color: C.ink, fontFace: 'Arial' } });

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

  // Title Lines
  slide.addText('Locality-Aware', {
    x: 0.8, y: 1.05, w: 6.5, h: 0.75,
    fontFace: 'Arial Black', fontSize: 44, color: C.ink, bold: true, valign: 'top'
  });
  slide.addText('Task Scheduling', {
    x: 0.8, y: 1.75, w: 6.5, h: 0.75,
    fontFace: 'Arial Black', fontSize: 44, color: C.ink, bold: true, valign: 'top'
  });

  // Canary Yellow Highlight Box behind "on Heterogeneous HPC"
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 0.8, y: 2.65, w: 5.4, h: 0.9,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('on Heterogeneous HPC', {
    x: 0.95, y: 2.65, w: 5.1, h: 0.9,
    fontFace: 'Arial Black', fontSize: 32, color: C.ink, bold: true, valign: 'middle'
  });

  // Pills below title
  const pills = [
    { text: 'AMD Opteron 8-Node NUMA', fill: C.cardWhite, textCol: C.ink, w: 2.3 },
    { text: 'TILEPro64 8×8 Mesh', fill: C.badgeBlue, textCol: C.ink, w: 1.9 },
    { text: 'NOVA Co-Scheduler', fill: C.badgePeach, textCol: C.ink, w: 1.9 }
  ];
  let currX = 0.8;
  pills.forEach(p => {
    slide.addShape(pptx.ShapeType.roundRect, {
      x: currX, y: 3.8, w: p.w, h: 0.35,
      fill: { color: p.fill },
      rectRadius: 0.17,
      line: { color: C.ink, width: 1.5 }
    });
    slide.addText(p.text, {
      x: currX, y: 3.8, w: p.w, h: 0.35,
      fontSize: 9.5, fontFace: 'Arial', bold: true, color: p.textCol, align: 'center', valign: 'middle'
    });
    currX += p.w + 0.12;
  });

  // Executive Description
  slide.addText(
    'An empirical reproduction of Muddukrishna et al. (Scientific Programming 2015), uncovering the SparseLU locality paradox (+15.1% slowdown), and delivering the NOVA co-scheduling runtime architecture for multi-socket servers and 2D mesh manycores.',
    {
      x: 0.8, y: 4.4, w: 5.4, h: 1.4,
      fontFace: 'Arial', fontSize: 11.5, color: C.inkLight, lineSpacing: 18, valign: 'top'
    }
  );

  // Presenter Footer
  addFooter(slide, 'Qasim · Awadesh · Devashish · Husaam · Chirag', '01');

  // RIGHT SIDE: Hero Architectural Comparison Card
  addNeoCard(slide, { x: 6.8, y: 1.1, w: 5.7, h: 5.4, fill: C.cardWhite, radius: 0.18 });

  slide.addText('Dual-Architecture Latency Cliffs', {
    x: 7.1, y: 1.3, w: 5.1, h: 0.4,
    fontFace: 'Arial Black', fontSize: 18, color: C.ink
  });
  slide.addText('Why blind work-stealing collapses on non-uniform memory topologies', {
    x: 7.1, y: 1.7, w: 5.1, h: 0.3,
    fontFace: 'Courier New', fontSize: 9.5, color: C.inkMuted
  });

  // Left Inner Box: NUMA
  addNeoCard(slide, { x: 7.1, y: 2.1, w: 2.45, h: 2.3, fill: C.cardBlue, radius: 0.12, shadow: false });
  slide.addText('AMD Opteron (NUMA)', {
    x: 7.2, y: 2.2, w: 2.25, h: 0.3, fontFace: 'Arial Black', fontSize: 11, color: C.ink
  });
  slide.addText('8 Sockets · 48 Cores', {
    x: 7.2, y: 2.48, w: 2.25, h: 0.25, fontFace: 'Courier New', fontSize: 8.5, color: C.inkMuted
  });
  slide.addText([
    { text: 'Local DRAM:\n', options: { bold: true, fontSize: 9.5, color: C.ink } },
    { text: '~40 cycles\n\n', options: { bold: true, fontSize: 13, color: '047857' } },
    { text: 'Remote Socket Bus:\n', options: { bold: true, fontSize: 9.5, color: C.ink } },
    { text: '240 – 340 cycles\n', options: { bold: true, fontSize: 13, color: 'B91C1C' } },
    { text: '(6× – 8× Latency Cliff)', options: { bold: false, fontSize: 8.5, color: C.inkMuted } }
  ], { x: 7.2, y: 2.75, w: 2.25, h: 1.5, fontFace: 'Arial' });

  // Right Inner Box: Manycore Mesh
  addNeoCard(slide, { x: 9.75, y: 2.1, w: 2.45, h: 2.3, fill: C.cardPeach, radius: 0.12, shadow: false });
  slide.addText('TILEPro64 (Mesh)', {
    x: 9.85, y: 2.2, w: 2.25, h: 0.3, fontFace: 'Arial Black', fontSize: 11, color: C.ink
  });
  slide.addText('8×8 Grid · 64 Cores', {
    x: 9.85, y: 2.48, w: 2.25, h: 0.25, fontFace: 'Courier New', fontSize: 8.5, color: C.inkMuted
  });
  slide.addText([
    { text: 'Local Tile L2 Bank:\n', options: { bold: true, fontSize: 9.5, color: C.ink } },
    { text: '10 cycles\n\n', options: { bold: true, fontSize: 13, color: '047857' } },
    { text: 'Remote Mesh Hop:\n', options: { bold: true, fontSize: 9.5, color: C.ink } },
    { text: '38 + 2·h cycles\n', options: { bold: true, fontSize: 13, color: 'B91C1C' } },
    { text: '(Up to 14 hops = 66 cyc)', options: { bold: false, fontSize: 8.5, color: C.inkMuted } }
  ], { x: 9.85, y: 2.75, w: 2.25, h: 1.5, fontFace: 'Arial' });

  // Dark Code Box at Bottom of Card
  addNeoCard(slide, { x: 7.1, y: 4.6, w: 5.1, h: 1.6, fill: C.cardDark, radius: 0.12, shadow: false });
  slide.addText([
    { text: '// The Fundamental Dilemma in HPC Schedulers:\n', options: { color: '94A3B8', fontSize: 9, fontFace: 'Courier New' } },
    { text: 'cost(X) = Σ (D[i] / line_size) × latency(X, i)\n', options: { color: C.yellow, fontSize: 10, bold: true, fontFace: 'Courier New' } },
    { text: '• Locality dealing cuts comm stalls by up to 80%...\n', options: { color: 'F8FAFC', fontSize: 9, fontFace: 'Arial' } },
    { text: '• BUT starves idle cores when queue backlog is ignored!\n', options: { color: 'FCA5A5', fontSize: 9, bold: true, fontFace: 'Arial' } },
    { text: 'Goal: Co-schedule memory locality AND core load balance.', options: { color: '38BDF8', fontSize: 9, bold: true, fontFace: 'Arial' } }
  ], { x: 7.25, y: 4.65, w: 4.8, h: 1.5 });
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
  // Yellow Box behind "Two Latency Cliffs"
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

  // Subtitle
  slide.addText('Discrete socket memory hierarchies vs. distributed shared L2 cache on an 8×8 mesh', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  // Left Card: AMD Opteron NUMA
  addNeoCard(slide, { x: 0.8, y: 2.0, w: 5.7, h: 3.75, fill: C.cardBlue, radius: 0.16 });
  slide.addText('AMD Opteron 8-Node NUMA', {
    x: 1.1, y: 2.2, w: 5.1, h: 0.35, fontFace: 'Arial Black', fontSize: 16, color: C.ink
  });
  // Badge inside card
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1, y: 2.6, w: 2.8, h: 0.26, fill: { color: C.cardWhite }, rectRadius: 0.13, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('Hierarchical HyperTransport Bus', {
    x: 1.1, y: 2.6, w: 2.8, h: 0.26, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const numaSpecs = [
    '• Topology: 8 discrete sockets (3–6 cores/node, 24–48 cores total)',
    '• Memory: Discrete physical DRAM channels attached to each socket',
    '• Local Access: ~40 CPU cycles (Node RAM via integrated memory controller)',
    '• Remote Access: 240 – 340 CPU cycles (Cross-socket HyperTransport bus)',
    '• Diameter: 3 hops maximum between opposing sockets',
    '• Distribution: First-touch page allocation (4 KB page granularity)',
    '• Pathology: Bus congestion under work-stealing drags 6×–8× stall penalty'
  ];
  slide.addText(numaSpecs.join('\n'), {
    x: 1.1, y: 3.0, w: 5.1, h: 2.5, fontFace: 'Arial', fontSize: 10.5, color: C.inkLight, lineSpacing: 18
  });

  // Right Card: TILEPro64 Manycore Mesh
  addNeoCard(slide, { x: 6.8, y: 2.0, w: 5.7, h: 3.75, fill: C.cardPeach, radius: 0.16 });
  slide.addText('TILEPro64 64-Tile Manycore Mesh', {
    x: 7.1, y: 2.2, w: 5.1, h: 0.35, fontFace: 'Arial Black', fontSize: 16, color: C.ink
  });
  // Badge inside card
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.1, y: 2.6, w: 2.8, h: 0.26, fill: { color: C.cardWhite }, rectRadius: 0.13, line: { color: C.ink, width: 1.2 }
  });
  slide.addText('8×8 2D Dynamic Mesh Network', {
    x: 7.1, y: 2.6, w: 2.8, h: 0.26, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
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
    x: 7.1, y: 3.0, w: 5.1, h: 2.5, fontFace: 'Arial', fontSize: 10.5, color: C.inkLight, lineSpacing: 18
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 5.95, w: 11.7, h: 0.8,
    titleText: 'Architectural Lesson:',
    bodyText: 'In NUMA, memory latency is tied to physical socket boundaries. In TILEPro64, it scales continuously with 2D geometric Manhattan distance. Uniform task schedulers fail catastrophically on both.'
  });

  addFooter(slide, 'Locality-Aware Task Scheduling · Hardware Topology & Latency Models', '02');
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

  // 3 Category Cards side-by-side
  const cardW = 3.73;

  // Card 1: Scheduling Pathologies (H1-H4)
  addNeoCard(slide, { x: 0.8, y: 1.95, w: cardW, h: 3.85, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Stealing & Placement', {
    x: 1.0, y: 2.1, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 2.42, w: 1.6, h: 0.22, fill: { color: C.badgeBlue }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H1 – H4', {
    x: 1.0, y: 2.42, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h1_h4 = [
    '• H1 (Static Steal Reach): Fixed vicinity size starves threads on Map or drags 6× remote stalls on Vecmul.',
    '• H2 (Queue-Level Stealing): Pops victim queue tail blindly; ignores whether another task in that queue has local data!',
    '• H3 (Load-Blind Placement): Dealer routes to argmin(comm_cost) without reading queue length; piles all work on 1 core.',
    '• H4 (Binary Threshold Cliffs): Brittle sum(D) > LLC/C gates trigger heavy O(N²) path with no calibration.'
  ];
  slide.addText(h1_h4.join('\n\n'), {
    x: 1.0, y: 2.75, w: cardW - 0.4, h: 2.9, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 14
  });

  // Card 2: Data Model Limits (H5-H7)
  addNeoCard(slide, { x: 0.8 + cardW + 0.25, y: 1.95, w: cardW, h: 3.85, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Data Model & Feedback', {
    x: 1.05 + cardW + 0.25, y: 2.1, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.05 + cardW + 0.25, y: 2.42, w: 1.6, h: 0.22, fill: { color: C.badgePeach }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H5 – H7', {
    x: 1.05 + cardW + 0.25, y: 2.42, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h5_h7 = [
    '• H5 (Fragile Footprints): Footprint vector D relies on OpenMP depend clauses. Incomplete clauses silently ruin placement.',
    '• H6 (No Feedback Loop): Data distribution is frozen at omp_malloc. Runtime never migrates hot mis-placed pages.',
    '• H7 (Coarse/Fine Menu Blind Spot): Rigid 2-item menu fails on irregular matrix blocks (SparseLU), causing severe regression.'
  ];
  slide.addText(h5_h7.join('\n\n'), {
    x: 1.05 + cardW + 0.25, y: 2.75, w: cardW - 0.4, h: 2.9, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 15
  });

  // Card 3: TILEPro64 Mesh Specifics (H8-H10)
  addNeoCard(slide, { x: 0.8 + (cardW + 0.25) * 2, y: 1.95, w: cardW, h: 3.85, fill: C.cardBlue, radius: 0.14 });
  slide.addText('TILEPro64 Mesh Bottlenecks', {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.1, w: cardW - 0.4, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.42, w: 1.6, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Hotspots H8 – H10', {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.42, w: 1.6, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const h8_h10 = [
    '• H8 (Static Home-Cache Trapping): Data is permanently locked to initial home cache; forces brutal locality vs balance conflict.',
    '• H9 (Center Bisection Contention): XY dimension-order routing concentrates traffic on center bisection links (tiles 3,3 to 4,4).',
    '• H10 (2D Coarse/Fine Mismatch): 1D line striping destroys 2D spatial locality for matrix stencils and blocked solvers.'
  ];
  slide.addText(h8_h10.join('\n\n'), {
    x: 1.1 + (cardW + 0.25) * 2, y: 2.75, w: cardW - 0.4, h: 2.9, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 15
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 5.95, w: 11.7, h: 0.8,
    titleText: 'Audit Discovery:',
    bodyText: 'Optimizing memory locality while ignoring queue backlog and interconnect contention is mathematically self-defeating on irregular task graphs.'
  });

  addFooter(slide, 'Auditing Muddukrishna et al. (2015) · Hotspots H1–H10 Taxonomy', '03');
}

// ==========================================
// SLIDE 4: EMPIRICAL FINDINGS & THE SPARSLU PARADOX
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 3', 'EMPIRICAL REPRODUCTION & BENCHMARKS');

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

  slide.addText('100 NUMA runs on 8-node Opteron + 56 configurations on 64-tile TILEPro64 mesh simulator', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  // LEFT COLUMN: AMD Opteron NUMA Figure 7 Reproduction
  addNeoCard(slide, { x: 0.8, y: 1.95, w: 6.0, h: 4.85, fill: C.cardWhite, radius: 0.16 });
  slide.addText('AMD Opteron Reproduction (100 Runs, 100% Verification)', {
    x: 1.0, y: 2.1, w: 5.6, h: 0.28, fontFace: 'Arial Black', fontSize: 11.5, color: C.ink
  });

  if (fs.existsSync(FIG7_PATH)) {
    slide.addImage({
      path: FIG7_PATH,
      x: 1.0, y: 2.45, w: 5.6, h: 2.7
    });
  }

  // Key NUMA takeaways below chart
  slide.addText([
    { text: '• Map: ', options: { bold: true, color: C.ink } },
    { text: '-45.1% Comm Cost (0.55×) · -19.0% Execution Cycles (0.81×)\n', options: { color: '047857', bold: true } },
    { text: '• Matmul: ', options: { bold: true, color: C.ink } },
    { text: '-22.5% Comm Cost (0.78×) · Runtime Parity (Balanced)\n', options: { color: C.inkLight } },
    { text: '• SparseLU: ', options: { bold: true, color: C.ink } },
    { text: '-26.7% Comm Cost BUT +15.1% SLOWER Execution! (The Paradox)', options: { color: 'B91C1C', bold: true } }
  ], {
    x: 1.0, y: 5.25, w: 5.6, h: 1.4, fontFace: 'Arial', fontSize: 9.5
  });

  // RIGHT COLUMN: The SparseLU Breakdown + TILEPro64 Results
  // Card 1: The SparseLU Breakdown (Dark Card)
  addNeoCard(slide, { x: 7.1, y: 1.95, w: 5.4, h: 2.5, fill: C.cardDark, radius: 0.14 });
  slide.addText('The SparseLU Pathology Explained', {
    x: 7.3, y: 2.1, w: 5.0, h: 0.3, fontFace: 'Arial Black', fontSize: 13, color: C.yellow
  });
  slide.addText([
    { text: '// Why locality dealing caused a +15.1% slowdown:\n', options: { color: '94A3B8', fontSize: 8.5, fontFace: 'Courier New' } },
    { text: '1. Irregular Task DAG: ', options: { color: 'FFFFFF', bold: true, fontSize: 9 } },
    { text: 'Tasks have skewed input dependencies.\n', options: { color: 'CBD5E1', fontSize: 9 } },
    { text: '2. Load-Blind Placement: ', options: { color: 'FFFFFF', bold: true, fontSize: 9 } },
    { text: 'Dealer sent all critical tasks to Node 0.\n', options: { color: 'CBD5E1', fontSize: 9 } },
    { text: '3. Worker Starvation: ', options: { color: 'FFFFFF', bold: true, fontSize: 9 } },
    { text: 'Queue 0 had 20 tasks; Queues 1–7 were empty!\n', options: { color: 'FCA5A5', fontSize: 9, bold: true } },
    { text: '4. Cost Penalty: ', options: { color: 'FFFFFF', bold: true, fontSize: 9 } },
    { text: '23 worker cores sat completely idle. Comm win was wiped out by starvation cycles!', options: { color: 'FCA5A5', fontSize: 9 } }
  ], {
    x: 7.3, y: 2.45, w: 5.0, h: 1.85, fontFace: 'Arial'
  });

  // Card 2: TILEPro64 Simulator Findings
  addNeoCard(slide, { x: 7.1, y: 4.6, w: 5.4, h: 2.2, fill: C.cardPeach, radius: 0.14 });
  slide.addText('TILEPro64 8×8 Mesh Simulation Findings', {
    x: 7.3, y: 4.75, w: 5.0, h: 0.3, fontFace: 'Arial Black', fontSize: 12.5, color: C.ink
  });

  const tileResults = [
    '• 80.3% Communication Reduction: LA+Coarse drops comm cost from 6.67M down to 1.31M cycles on Map.',
    '• 39.3% Execution Speedup: Simulated runtime drops from 281K down to 170K cycles.',
    '• Steal Elimination: Drops from 63 remote mesh steals in Work-Stealing down to ZERO steals under LA+Coarse!',
    '• Mesh Latency Accuracy: Verified against 10-cycle local L2 and 38+2·hop remote formulas across all 64 tiles.'
  ];
  slide.addText(tileResults.join('\n'), {
    x: 7.3, y: 5.1, w: 5.0, h: 1.6, fontFace: 'Arial', fontSize: 9, color: C.inkLight, lineSpacing: 13
  });

  addFooter(slide, 'Empirical Reproduction: AMD Opteron NUMA & TILEPro64 Manycore Mesh', '04');
}

// ==========================================
// SLIDE 5: THE NOVA / ALLOC SOLUTION
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 4', 'SCHEDULER ARCHITECTURE');

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

  const cardW = 5.7;
  const cardH = 1.95;

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
    x: 1.0, y: 2.38, w: 5.3, h: 0.38, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('score(q) = α · comm_norm(D, q) + (1 - α) · occ_norm(q)', {
    x: 1.0, y: 2.38, w: 5.3, h: 0.38, fontFace: 'Courier New', fontSize: 9.5, bold: true, color: C.ink, align: 'center', valign: 'middle'
  });
  slide.addText('Blends data proximity with real-time queue backlog. Automatically diverts tasks to neighboring idle queues before congestion creates worker starvation.', {
    x: 1.0, y: 2.82, w: 5.3, h: 0.95, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 13
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
    x: 7.0, y: 2.38, w: 5.3, h: 0.38, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('steal_target = argmax_{t ∈ Top-K} ( Affinity(thief_node, Footprint(t)) )', {
    x: 7.0, y: 2.38, w: 5.3, h: 0.38, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: C.ink, align: 'center', valign: 'middle'
  });
  slide.addText('Replaces blind queue-tail popping with a lock-free Shadow Index. Idle thief inspects top k=8 tasks in victim queue and steals the task that has maximal affinity to thief RAM.', {
    x: 7.0, y: 2.82, w: 5.3, h: 0.95, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 13
  });

  // Pillar 3: N-C Dynamic Vicinity Radius (Fixes H1)
  addNeoCard(slide, { x: 0.8, y: 4.0, w: cardW, h: cardH, fill: C.cardGreen, radius: 0.14 });
  slide.addText('Pillar 3: Adaptive Vicinity Radius (N-C)', {
    x: 1.0, y: 4.1, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 4.8, y: 4.1, w: 1.5, h: 0.22, fill: { color: C.cardWhite }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H1', {
    x: 4.8, y: 4.1, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 4.43, w: 5.3, h: 0.38, fill: { color: C.cardWhite }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('radius_{t+1} = clamp( radius_t + sign(rate_{EWMA} - target), 1, D_max )', {
    x: 1.0, y: 4.43, w: 5.3, h: 0.38, fontFace: 'Courier New', fontSize: 8.5, bold: true, color: C.ink, align: 'center', valign: 'middle'
  });
  slide.addText('Replaces fixed hard-coded steal radius with per-worker EWMA feedback. Expands reach during starvation; contracts to local cluster when queues are well-balanced.', {
    x: 1.0, y: 4.87, w: 5.3, h: 0.95, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 13
  });

  // Pillar 4: N-D / H8 Dynamic Home-Cache Migration & Zero Locks
  addNeoCard(slide, { x: 6.8, y: 4.0, w: cardW, h: cardH, fill: C.cardWhite, radius: 0.14 });
  slide.addText('Pillar 4: Dynamic Migration & Zero Locks', {
    x: 7.0, y: 4.1, w: 4.0, h: 0.28, fontFace: 'Arial Black', fontSize: 12, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 10.8, y: 4.1, w: 1.5, h: 0.22, fill: { color: C.badgePeach }, rectRadius: 0.11, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Fixes H6, H8', {
    x: 10.8, y: 4.1, w: 1.5, h: 0.22, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 4.43, w: 5.3, h: 0.38, fill: { color: C.cardBlue }, rectRadius: 0.08, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('Trigger Migration IF (Acc_Benefit > C_mig ≈ 120 cycles)', {
    x: 7.0, y: 4.43, w: 5.3, h: 0.38, fontFace: 'Courier New', fontSize: 9.5, bold: true, color: C.ink, align: 'center', valign: 'middle'
  });
  slide.addText('Dynamic home-cache line re-homing toward executing tiles. Coordination telemetry uses double-buffered statistics swapped via atomic CAS every 64 ops — zero locks on the hot path!', {
    x: 7.0, y: 4.87, w: 5.3, h: 0.95, fontFace: 'Arial', fontSize: 9.5, color: C.inkLight, lineSpacing: 13
  });

  // Bottom Callout Banner
  addCalloutBanner(slide, {
    x: 0.8, y: 6.05, w: 11.7, h: 0.75,
    titleText: 'NOVA Principle:',
    bodyText: 'Maximize memory locality when queues are balanced; dynamically transition to load-balancing when queue backlog or interconnect contention threatens idle core stalls.'
  });

  addFooter(slide, 'NOVA / ALLoC Scheduler Architecture · High-Performance Computing', '05');
}

// ==========================================
// SLIDE 6: SUMMARY, DELIVERABLES & DEFENSE Q&A
// ==========================================
{
  const slide = pptx.addSlide();
  applySlideBase(slide);
  addHeaderBadges(slide, 'PART 5', 'SUMMARY & DEFENSE READY');

  // Title
  slide.addText('Project Deliverables & ', {
    x: 0.8, y: 0.95, w: 6.0, h: 0.55,
    fontFace: 'Arial Black', fontSize: 30, color: C.ink, bold: true
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 6.6, y: 0.9, w: 5.5, h: 0.65,
    fill: { color: C.yellow },
    rectRadius: 0.1,
    line: { color: C.ink, width: 1.5 }
  });
  slide.addText('Examiner Defense Q&A', {
    x: 6.7, y: 0.9, w: 5.3, h: 0.65,
    fontFace: 'Arial Black', fontSize: 28, color: C.ink, bold: true, align: 'center', valign: 'middle'
  });

  slide.addText('Full dual-architecture reproduction, standalone manycore simulator, and NOVA scheduler specification', {
    x: 0.8, y: 1.6, w: 10.0, h: 0.25,
    fontFace: 'Courier New', fontSize: 10, color: C.inkMuted
  });

  // LEFT COLUMN: Concrete Deliverables
  addNeoCard(slide, { x: 0.8, y: 1.95, w: 5.7, h: 4.85, fill: C.cardWhite, radius: 0.16 });
  slide.addText('Summary of Concrete Deliverables', {
    x: 1.0, y: 2.1, w: 5.3, h: 0.35, fontFace: 'Arial Black', fontSize: 15, color: C.ink
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 1.0, y: 2.5, w: 2.2, h: 0.25, fill: { color: C.badgeGreen }, rectRadius: 0.12, line: { color: C.ink, width: 1.0 }
  });
  slide.addText('100% COMPLETE & VERIFIED', {
    x: 1.0, y: 2.5, w: 2.2, h: 0.25, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.ink, align: 'center', valign: 'middle'
  });

  const deliverables = [
    '1. AMD Opteron NUMA Baseline Reproduction:',
    '   100 benchmark runs across 5 kernels (Map, Matmul, Reduce, Jacobi, SparseLU); verified Figure 7 reproduction with check_ok=1.',
    '',
    '2. Standalone TILEPro64 8×8 Mesh Simulator:',
    '   Built full C++/Python mesh simulator from scratch; modeled 10 vs 38+2·hop cycles; 11/11 unit tests passing.',
    '',
    '3. Empirical Discovery of SparseLU Paradox:',
    '   Identified and proved the queue starvation pathology that causes locality dealing to run +15.1% slower than work-stealing.',
    '',
    '4. 10-Hotspot Audit Taxonomy (H1–H10):',
    '   Comprehensive taxonomy covering NUMA steal limits and TILEPro64 2D mesh bisection bottlenecks.',
    '',
    '5. Full NOVA / ALLoC Scheduler Architecture:',
    '   Complete mathematical formulation, shadow index stealing algorithm, and lock-free double-buffered control plane.',
    '',
    '6. Open-Source Codebase & Git PR #3:',
    '   Fully documented and reproducible on branch feat/tilepro64-simulation-and-presentation.'
  ];
  slide.addText(deliverables.join('\n'), {
    x: 1.0, y: 2.85, w: 5.3, h: 3.8, fontFace: 'Arial', fontSize: 8.8, color: C.inkLight, lineSpacing: 12
  });

  // RIGHT COLUMN: Anticipated Examiner Q&A
  addNeoCard(slide, { x: 6.8, y: 1.95, w: 5.7, h: 4.85, fill: C.cardDark, radius: 0.16 });
  slide.addText('Anticipated Examiner Questions & Answers', {
    x: 7.0, y: 2.1, w: 5.3, h: 0.35, fontFace: 'Arial Black', fontSize: 14, color: C.yellow
  });
  slide.addShape(pptx.ShapeType.roundRect, {
    x: 7.0, y: 2.5, w: 2.0, h: 0.25, fill: { color: C.inkLight }, rectRadius: 0.12, line: { color: C.yellow, width: 1.0 }
  });
  slide.addText('EXAMINER DEFENSE READY', {
    x: 7.0, y: 2.5, w: 2.0, h: 0.25, fontSize: 8, fontFace: 'Courier New', bold: true, color: C.yellow, align: 'center', valign: 'middle'
  });

  const qaText = [
    { text: 'Q1: Why simulate TILEPro64 instead of running on real hardware?\n', options: { color: 'FFFFFF', bold: true, fontSize: 9.5 } },
    { text: 'Ans: TILEPro64 is discontinued specialty hardware. A software model implementing the exact 8×8 mesh and 2D Manhattan latency matrix (10 cycles local, 38+2·hop remote) enables deterministic, cycle-accurate evaluation without hardware drift.\n\n', options: { color: 'CBD5E1', fontSize: 9 } },
    { text: 'Q2: Why does work-stealing suffer 63 steals in Map while LA+Coarse has 0?\n', options: { color: 'FFFFFF', bold: true, fontSize: 9.5 } },
    { text: 'Ans: In work-stealing, Tile 0 creates all 64 tasks; the other 63 idle tiles must each steal remotely across the mesh. Under LA+Coarse, the dealer immediately routes each task to its home tile, eliminating steal overhead entirely.\n\n', options: { color: 'CBD5E1', fontSize: 9 } },
    { text: 'Q3: How does NOVA avoid lock contention on the critical path?\n', options: { color: 'FFFFFF', bold: true, fontSize: 9.5 } },
    { text: 'Ans: Workers make dealing and stealing decisions locally in O(nodes) time without locks. Global queue snapshots and α tuning run in a double-buffer swapped via atomic pointer CAS every 64 ops — zero locks on the hot execution path.\n\n', options: { color: 'CBD5E1', fontSize: 9 } },
    { text: 'Q4: What is the main takeaway for future HPC runtimes?\n', options: { color: 'FFFFFF', bold: true, fontSize: 9.5 } },
    { text: 'Ans: Memory locality and load balance cannot be optimized independently. Adaptive co-scheduling is strictly necessary for scalable speedup.', options: { color: C.yellow, bold: true, fontSize: 9 } }
  ];
  slide.addText(qaText, {
    x: 7.0, y: 2.85, w: 5.3, h: 3.8, fontFace: 'Arial'
  });

  addFooter(slide, 'Final HPC Project Defense · Qasim · Awadesh · Devashish · Husaam · Chirag', '06');
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
