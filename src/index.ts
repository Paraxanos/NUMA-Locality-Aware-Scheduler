import PptxGenJS from 'pptxgenjs';
import { SLIDE_W, SLIDE_H, PAPER, INK, CANARY, DARKNAVY, FONTS } from './theme';

export const pres = new PptxGenJS();
pres.defineLayout({ name: 'CUSTOM_16_9', width: SLIDE_W, height: SLIDE_H });
pres.layout = 'CUSTOM_16_9';

function addGrid(slide: PptxGenJS.Slide) {
  // Faint engineering grid matching docs/bg_grid.png
  slide.addShape(pres.ShapeType.rect, { x: 0, y: 0, w: SLIDE_W, h: SLIDE_H, fill: { color: PAPER }, line: { color: PAPER, width: 0 } });
  for (let i = 1; i < 12; i++) {
    slide.addShape(pres.ShapeType.rect, { x: i * 1.05, y: 0, w: 0.01, h: SLIDE_H, fill: { color: '#DDD8CF' }, line: { color: '#DDD8CF', width: 0 } });
  }
  for (let j = 1; j < 7; j++) {
    slide.addShape(pres.ShapeType.rect, { x: 0, y: j * 1.05, w: SLIDE_W, h: 0.01, fill: { color: '#DDD8CF' }, line: { color: '#DDD8CF', width: 0 } });
  }
}

function footer(slide: PptxGenJS.Slide, text: string, n: number) {
  slide.addText(text + '  ·  ' + String(n).padStart(2,'0') + ' / 05', {
    x: 0.45, y: 7.15, w: 12.4, h: 0.2,
    fontSize: 9, color: '#6b6b6b', fontFace: FONTS.mono,
  });
}

// =========== SLIDE 1: HERO ==========
{
  const s = pres.addSlide();
  addGrid(s);
  // Dark pill badges (reference p01 style)
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
  s.addText('HPC SYSTEMS', { x: 0.55, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });

  s.addShape(pres.ShapeType.roundRect, { x: 3.2, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: PAPER }, line: { color: INK, width: 1.5 } });
  s.addText('RESEARCH REPRODUCTION', { x: 3.3, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: INK, charSpacing: 2, fontFace: FONTS.mono });

  // Large condensed headline
  s.addText('Locality-Aware\nTask Scheduling', {
    x: 0.45, y: 1.0, w: 7.5, h: 2.6,
    fontSize: 56, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -3, lineSpacing: 52,
  });
  // Canary highlight box on 3rd word
  s.addShape(pres.ShapeType.roundRect, {
    x: 0.45, y: 3.55, w: 7.5, h: 0.85, rectRadius: 0.08,
    fill: { color: CANARY }, line: { color: INK, width: 2.5 },
  });
  s.addText('on Heterogeneous Architectures', {
    x: 0.65, y: 3.6, w: 7.1, h: 0.75,
    fontSize: 36, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -2, valign: 'middle',
  });

  // Three pills
  const pills = ['AMD Opteron 8-Node NUMA', 'TILEPro64 8×8 Mesh', 'NOVA / ALLoC'];
  pills.forEach((p, i) => {
    const x = 0.45 + i * 2.7;
    s.addShape(pres.ShapeType.roundRect, { x, y: 4.75, w: 2.5, h: 0.4, rectRadius: 0.2, fill: { color: '#EFEAE3' }, line: { color: INK, width: 1.5 } });
    s.addText(p, { x: x + 0.1, y: 4.75, w: 2.3, h: 0.4, fontSize: 11, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.body });
  });

  // Right architectural diagram (simplified: 8-node + mesh nodes with connections)
  const diagX = 9.0, diagY = 1.0, diagW = 3.8, diagH = 4.0;
  s.addShape(pres.ShapeType.roundRect, { x: diagX, y: diagY, w: diagW, h: diagH, rectRadius: 0.1, fill: { color: '#EFEAE3' }, line: { color: INK, width: 2 } });
  s.addText('DUAL ARCHITECTURE', { x: diagX + 0.15, y: diagY + 0.15, w: 3.5, h: 0.3, fontSize: 10, bold: true, color: INK, charSpacing: 2, fontFace: FONTS.mono });

  // NUMA 8-node diagram (left half of diagram)
  s.addShape(pres.ShapeType.ellipse, { x: diagX + 0.3, y: diagY + 0.8, w: 1.0, h: 0.7, fill: { color: DARKNAVY }, line: { color: INK, width: 1 } });
  s.addText('NUMA\n8-node', { x: diagX + 0.3, y: diagY + 0.9, w: 1.0, h: 0.5, fontSize: 9, bold: true, color: CANARY, align: 'center', valign: 'middle', fontFace: FONTS.mono });

  // Mesh 8x8 (right half of diagram)
  s.addShape(pres.ShapeType.rect, { x: diagX + 2.0, y: diagY + 0.8, w: 1.5, h: 1.0, fill: { color: PAPER }, line: { color: INK, width: 1.5 } });
  s.addText('MESH\n8×8', { x: diagX + 2.0, y: diagY + 1.0, w: 1.5, h: 0.6, fontSize: 9, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.mono });

  // Latency labels
  s.addText('Local ~40 cyc  ·  Remote 240–340 cyc', { x: diagX + 0.3, y: diagY + 2.2, w: 3.2, h: 0.4, fontSize: 10, color: INK, fontFace: FONTS.body });
  s.addText('Local 10 cyc  ·  Remote 38+2·h cyc', { x: diagX + 0.3, y: diagY + 2.7, w: 3.2, h: 0.4, fontSize: 10, color: INK, fontFace: FONTS.body });

  // Bottom quote / subtitle
  s.addText('Reproducing Muddukrishna et al. (Scientific Programming 2015) — uncovering the SparseLU paradox and delivering NOVA co-scheduling.', {
    x: 0.45, y: 6.0, w: 8.2, h: 0.7, fontSize: 13, color: INK, fontFace: FONTS.body, italic: true,
  });
  footer(s, 'Locality-Aware Task Scheduling on Heterogeneous HPC Architectures', 1);
}

// =========== SLIDE 2: HARDWARE FOUNDATIONS ==========
{
  const s = pres.addSlide();
  addGrid(s);
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
  s.addText('PART 01', { x: 0.55, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });

  s.addText('Two Memory Models,', { x: 0.45, y: 0.9, w: 12, h: 0.9, fontSize: 48, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -3 });
  s.addShape(pres.ShapeType.roundRect, { x: 5.9, y: 1.7, w: 6.6, h: 0.7, rectRadius: 0.1, fill: { color: CANARY }, line: { color: INK, width: 2.5 } });
  s.addText('Two Latency Cliffs', { x: 6.0, y: 1.75, w: 6.4, h: 0.6, fontSize: 36, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -2, valign: 'middle' });
  s.addText('Discrete socket memory hierarchies vs. distributed shared L2 cache on an 8×8 mesh', { x: 0.45, y: 2.6, w: 12, h: 0.4, fontSize: 14, color: '#4B5563', fontFace: FONTS.body, italic: true });

  // Two columns
  const colW = 5.9;
  // Left card: NUMA
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 3.15, w: colW, h: 3.4, rectRadius: 0.08, fill: { color: PAPER }, line: { color: INK, width: 2.5 }, shadow: { type: 'outer', color: 'C8C6BF', pt: 3, blur: 4 } });
  s.addText('AMD Opteron 8-Node NUMA', { x: 0.75, y: 3.3, w: 5.3, h: 0.4, fontSize: 20, bold: true, color: INK, fontFace: FONTS.display });
  s.addText('HyperTransport Interconnect  ·  3–6 cores/node  ·  First-touch allocation', { x: 0.75, y: 3.75, w: 5.3, h: 0.3, fontSize: 11, color: '#374151', fontFace: FONTS.body });
  // Spec list
  const numaSpecs = [
    'Topology: 8 sockets · 24–48 cores total',
    'Local DRAM: ~40 CPU cycles (integrated IMC)',
    'Remote Bus: 240–340 CPU cycles (6×–8× stall)',
    'Diameter: 3 hops max  ·  Failure: bus saturation',
  ];
  numaSpecs.forEach((txt, i) => {
    s.addShape(pres.ShapeType.roundRect, { x: 0.75, y: 4.15 + i * 0.42, w: 5.3, h: 0.32, rectRadius: 0.06, fill: { color: '#EFEAE3' }, line: { color: '#ddd8cf', width: 1 } });
    s.addText(txt, { x: 0.9, y: 4.15 + i * 0.42, w: 5.0, h: 0.32, fontSize: 11, color: INK, fontFace: FONTS.body, valign: 'middle' });
  });

  // Right card: TILEPro64
  s.addShape(pres.ShapeType.roundRect, { x: 6.8, y: 3.15, w: colW, h: 3.4, rectRadius: 0.08, fill: { color: '#111827' }, line: { color: INK, width: 2.5 }, shadow: { type: 'outer', color: 'C8C6BF', pt: 3, blur: 4 } });
  s.addText('TILEPro64 8×8 Mesh', { x: 7.1, y: 3.3, w: 5.3, h: 0.4, fontSize: 20, bold: true, color: CANARY, fontFace: FONTS.display });
  s.addText('Manhattan hop distance  ·  64 tiles  ·  2D dynamic mesh', { x: 7.1, y: 3.75, w: 5.3, h: 0.3, fontSize: 11, color: '#AAB2C7', fontFace: FONTS.body });
  const meshSpecs = [
    'Topology: 8×8 grid · 1 VLIW core per tile',
    'Local L2: 10 cycles  ·  Remote: 38 + 2·h',
    'Diameter: 14 hops = 66 cycles (opposite corner)',
    'Failure: center bisection contention / home trap',
  ];
  meshSpecs.forEach((txt, i) => {
    s.addShape(pres.ShapeType.roundRect, { x: 7.1, y: 4.15 + i * 0.42, w: 5.3, h: 0.32, rectRadius: 0.06, fill: { color: '#1a2230' }, line: { color: '#334155', width: 1 } });
    s.addText(txt, { x: 7.25, y: 4.15 + i * 0.42, w: 5.0, h: 0.32, fontSize: 11, color: PAPER, fontFace: FONTS.body, valign: 'middle' });
  });

  // Dark code block (reference style)
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 6.55, w: 12.4, h: 0.55, rectRadius: 0.06, fill: { color: DARKNAVY }, line: { color: INK, width: 1.5 } });
  s.addText('cost(X) = Σ (D[i] / line_size) × latency(X, i)  ·  locality dealing cuts stalls up to 80% — but starves idle cores when backlog is ignored!', {
    x: 0.75, y: 6.6, w: 11.8, h: 0.45, fontSize: 11, color: CANARY, fontFace: FONTS.mono, valign: 'middle',
  });
  footer(s, 'Locality-Aware Task Scheduling · Hardware Topology & Latency Models', 2);
}

// =========== SLIDE 3: 10 HOTSPOTS ==========
{
  const s = pres.addSlide();
  addGrid(s);
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
  s.addText('PART 02', { x: 0.55, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });

  s.addText('Why Naive Locality Fails:', { x: 0.45, y: 0.85, w: 12, h: 0.7, fontSize: 42, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -3 });
  s.addShape(pres.ShapeType.roundRect, { x: 7.2, y: 1.5, w: 5.6, h: 0.6, rectRadius: 0.08, fill: { color: CANARY }, line: { color: INK, width: 2.5 } });
  s.addText('The 10 Hotspots', { x: 7.3, y: 1.55, w: 5.4, h: 0.5, fontSize: 32, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -2, valign: 'middle' });
  s.addText('Deep audit of Muddukrishna et al. (2015) — root causes of runtime regression', { x: 0.45, y: 2.2, w: 12, h: 0.3, fontSize: 14, color: '#4B5563', fontFace: FONTS.body, italic: true });

  // Three column cards for H1-H4 / H5-H7 / H8-H10
  const cards = [
    { title: 'Stealing & Placement', sub: 'H1 – H4', items: [
      'H1 Static Steal Reach: hard-coded vicinity starves threads on Map or drags remote stalls.',
      'H2 Queue-Level Stealing: pops victim tail blindly — ignores local-data tasks in same queue.',
      'H3 Load-Blind Placement: dealer routes to argmin(comm) without reading queue length.',
      'H4 Binary Threshold Cliffs: hard gates trigger O(N²) path with no calibration.',
    ], highlight: false },
    { title: 'Data Model & Feedback', sub: 'H5 – H7', items: [
      'H5 Fragile Footprints: D relies on OpenMP depend clauses; incomplete clauses ruin placement.',
      'H6 No Feedback Loop: data frozen at omp_malloc; runtime never migrates hot mis-placed pages.',
      'H7 Coarse/Fine Menu Blind: rigid 2-item menu fails on irregular SparseLU blocks.',
    ], highlight: false },
    { title: 'Mesh Bottlenecks', sub: 'H8 – H10', items: [
      'H8 Static Home-Cache Trapping: data locked to initial tile — brutal locality vs load trade-off.',
      'H9 Center Bisection Contention: XY routing concentrates traffic at tiles 3,3 → 4,4.',
      'H10 2D Coarse/Fine Mismatch: 1D line striping destroys 2D spatial locality for block solvers.',
    ], highlight: true },
  ];
  cards.forEach((c, i) => {
    const x = 0.45 + i * 4.15;
    const bg = c.highlight ? '#111827' : PAPER;
    const textCol = c.highlight ? PAPER : INK;
    const subCol = c.highlight ? CANARY : '#1a6fb5';
    const borderCol = INK;
    s.addShape(pres.ShapeType.roundRect, { x, y: 2.7, w: 3.9, h: 3.6, rectRadius: 0.08, fill: { color: bg }, line: { color: borderCol, width: 2.5 }, shadow: { type: 'outer', color: 'C8C6BF', pt: 3, blur: 4 } });
    s.addText(c.title, { x: x + 0.2, y: 2.9, w: 3.5, h: 0.3, fontSize: 18, bold: true, color: textCol, fontFace: FONTS.display });
    s.addShape(pres.ShapeType.roundRect, { x: x + 0.2, y: 3.25, w: 1.2, h: 0.28, rectRadius: 0.14, fill: { color: subCol }, line: { color: subCol, width: 0 } });
    s.addText(c.sub, { x: x + 0.2, y: 3.25, w: 1.2, h: 0.28, fontSize: 9, bold: true, color: bg === '#111827' ? INK : PAPER, align: 'center', valign: 'middle', fontFace: FONTS.mono });
    c.items.forEach((it, j) => {
      s.addText('•  ' + it, { x: x + 0.2, y: 3.7 + j * 0.65, w: 3.5, h: 0.6, fontSize: 10.5, color: textCol, fontFace: FONTS.body, lineSpacing: 11 });
    });
  });

  // Yellow banner at bottom
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 6.45, w: 12.4, h: 0.65, rectRadius: 0.06, fill: { color: CANARY }, line: { color: INK, width: 2.5 }, shadow: { type: 'outer', color: INK, pt: 3, blur: 4 } });
  s.addText('AUDIT DISCOVERY  ·  Optimizing memory locality while ignoring queue backlog and interconnect contention is mathematically self-defeating on irregular task graphs.', {
    x: 0.7, y: 6.5, w: 11.9, h: 0.55, fontSize: 14, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.body,
  });
  footer(s, 'Auditing Muddukrishna et al. (2015) · 10 Hotspots Taxonomy H1–H10', 3);
}

// =========== SLIDE 4: SPARSELU PARADOX ==========
{
  const s = pres.addSlide();
  addGrid(s);
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
  s.addText('PART 03', { x: 0.55, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });

  s.addText('The SparseLU Paradox:', { x: 0.45, y: 0.85, w: 12, h: 0.8, fontSize: 42, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -3 });
  s.addShape(pres.ShapeType.roundRect, { x: 6.5, y: 1.55, w: 6.4, h: 0.7, rectRadius: 0.08, fill: { color: CANARY }, line: { color: INK, width: 2.5 } });
  s.addText('When Locality Slows Down', { x: 6.6, y: 1.6, w: 6.2, h: 0.6, fontSize: 30, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -2, valign: 'middle' });
  s.addText('100 NUMA runs (8-node Opteron) + 56 TILEPro64 mesh configurations', { x: 0.45, y: 2.35, w: 12, h: 0.3, fontSize: 14, color: '#4B5563', fontFace: FONTS.body, italic: true });

  // Left: embedded chart image
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 2.85, w: 7.2, h: 3.7, rectRadius: 0.08, fill: { color: PAPER }, line: { color: INK, width: 2.5 }, shadow: { type: 'outer', color: 'C8C6BF', pt: 3, blur: 4 } });
  s.addText('AMD Opteron Reproduction (100 Runs · 100% Verification)', { x: 0.75, y: 3.0, w: 6.6, h: 0.3, fontSize: 15, bold: true, color: INK, fontFace: FONTS.display });
  s.addImage({ path: './results/numa/fig7_reproduction.png', x: 0.75, y: 3.35, w: 6.6, h: 2.4 });
  // Caption bullets
  s.addShape(pres.ShapeType.roundRect, { x: 0.75, y: 5.8, w: 6.6, h: 0.6, rectRadius: 0.06, fill: { color: '#EFEAE3' }, line: { color: INK, width: 1 } });
  s.addText('•  Map: −45.1% Comm (0.55×) · −19.0% Cycles (0.81×)    •  Matmul: −22.5% Comm (0.78×) · Parity    •  SparseLU: −26.7% Comm BUT +15.1% SLOWER!', { x: 0.9, y: 5.85, w: 6.3, h: 0.5, fontSize: 10, color: INK, fontFace: FONTS.mono, valign: 'middle' });

  // Right: dark pathology card + tilepro64 card
  s.addShape(pres.ShapeType.roundRect, { x: 7.9, y: 2.85, w: 5.0, h: 2.0, rectRadius: 0.08, fill: { color: DARKNAVY }, line: { color: INK, width: 2.5 } });
  s.addText('THE PARADOX EXPLAINED', { x: 8.15, y: 3.0, w: 4.5, h: 0.3, fontSize: 14, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });
  const pathLines = [
    '1. Irregular DAG: tasks have skewed input dependencies.',
    '2. Load-blind dealer sends critical tasks to Node 0.',
    '3. Queue 0: 20 tasks  ·  Queues 1–7: EMPTY.',
    '4. 23 cores idle — comm win wiped by starvation.',
  ];
  pathLines.forEach((ln, i) => {
    s.addText(ln, { x: 8.15, y: 3.35 + i * 0.32, w: 4.5, h: 0.3, fontSize: 11, color: PAPER, fontFace: FONTS.mono, valign: 'middle' });
  });

  s.addShape(pres.ShapeType.roundRect, { x: 7.9, y: 5.0, w: 5.0, h: 1.55, rectRadius: 0.08, fill: { color: '#EFEAE3' }, line: { color: INK, width: 2.5 } });
  s.addText('TILEPro64 8×8 Mesh', { x: 8.15, y: 5.1, w: 4.5, h: 0.3, fontSize: 16, bold: true, color: INK, fontFace: FONTS.display });
  s.addText('• 80.3% comm reduction (6.67M → 1.31M cycles)  ·  • 39.3% speedup (281K → 170K cycles)  ·  • Zero remote steals (63 → 0)', { x: 8.15, y: 5.45, w: 4.5, h: 0.8, fontSize: 11, color: INK, fontFace: FONTS.body, lineSpacing: 13 });
  s.addText('+15.1% execution slowdown vs −26.7% comm cost drop — the paradox', { x: 8.15, y: 6.25, w: 4.5, h: 0.25, fontSize: 9, bold: true, color: '#B91C1C', fontFace: FONTS.mono });

  footer(s, 'Empirical Reproduction: AMD Opteron NUMA & TILEPro64 Manycore Mesh', 4);
}

// =========== SLIDE 5: NOVA SOLUTION & DEFENSE ==========
{
  const s = pres.addSlide();
  addGrid(s);
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 0.35, w: 2.6, h: 0.32, rectRadius: 0.16, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
  s.addText('PART 04', { x: 0.55, y: 0.35, w: 2.4, h: 0.32, fontSize: 9, bold: true, color: CANARY, charSpacing: 2, fontFace: FONTS.mono });

  s.addText('The NOVA Solution:', { x: 0.45, y: 0.85, w: 12, h: 0.7, fontSize: 42, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -3 });
  s.addShape(pres.ShapeType.roundRect, { x: 5.5, y: 1.55, w: 7.4, h: 0.6, rectRadius: 0.08, fill: { color: CANARY }, line: { color: INK, width: 2.5 } });
  s.addText('Adaptive Co-Scheduling', { x: 5.6, y: 1.6, w: 7.2, h: 0.5, fontSize: 32, bold: true, color: INK, fontFace: FONTS.display, charSpacing: -2, valign: 'middle' });
  s.addText('Four architectural pillars + defense takeaways — zero-contention control plane', { x: 0.45, y: 2.3, w: 12, h: 0.3, fontSize: 14, color: '#4B5563', fontFace: FONTS.body, italic: true });

  // 2x2 pillar cards
  const pillars = [
    { label: 'N-A', title: 'Load-Aware Work Dealing', fix: 'Fixes H3, H4', body: 'Composite score:  α·comm_norm(D,q) + (1−α)·occ_norm(q)  — balances proximity with backlog.', code: 'score(q) = α·comm_norm + (1−α)·occ_norm' },
    { label: 'N-B', title: 'Task-Level Shadow Index', fix: 'Fixes H2', body: 'Lock-free ring buffer (k=8). Thief peeks top tasks — steals matching local RAM instead of blind tail-pop.', code: 'argmax_{t∈Top-K} Affinity(thief, Footprint(t))' },
    { label: 'N-C', title: 'Adaptive Vicinity Radius', fix: 'Fixes H1', body: 'Per-worker EWMA adjusts reach: expands during starvation, contracts when balanced.', code: 'radius_{t+1} = clamp(radius_t + sign(rate_EWMA−target), 1, D_max)' },
    { label: 'N-D', title: 'Dynamic Migration & Zero Locks', fix: 'Fixes H6, H8', body: 'Re-home lines when Acc_Benefit > C_mig (~120 cyc). Double-buffered CAS swap every 64 ops.', code: 'Trigger IF (Acc_Benefit > 120 cycles)' },
  ];
  pillars.forEach((p, i) => {
    const col = i % 2, row = Math.floor(i / 2);
    const x = 0.45 + col * 6.3;
    const y = 2.7 + row * 1.85;
    const bg = (i % 2 === 0) ? PAPER : '#EFEAE3';
    s.addShape(pres.ShapeType.roundRect, { x, y, w: 6.0, h: 1.65, rectRadius: 0.08, fill: { color: bg }, line: { color: INK, width: 2 }, shadow: { type: 'outer', color: 'C8C6BF', pt: 2, blur: 3 } });
    s.addShape(pres.ShapeType.ellipse, { x: x + 0.2, y: y + 0.25, w: 0.9, h: 0.55, fill: { color: CANARY }, line: { color: INK, width: 1.5 } });
    s.addText(p.label, { x: x + 0.2, y: y + 0.25, w: 0.9, h: 0.55, fontSize: 16, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.display });
    s.addText(p.title, { x: x + 1.2, y: y + 0.25, w: 4.6, h: 0.35, fontSize: 16, bold: true, color: INK, fontFace: FONTS.display });
    s.addShape(pres.ShapeType.roundRect, { x: x + 1.2, y: y + 0.65, w: 1.0, h: 0.25, rectRadius: 0.12, fill: { color: '#DCE4FF' }, line: { color: '#DCE4FF', width: 0 } });
    s.addText(p.fix, { x: x + 1.2, y: y + 0.65, w: 1.0, h: 0.25, fontSize: 9, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.mono });
    s.addText(p.body, { x: x + 1.2, y: y + 0.95, w: 4.6, h: 0.35, fontSize: 11, color: '#374151', fontFace: FONTS.body, lineSpacing: 11 });
    s.addShape(pres.ShapeType.roundRect, { x: x + 0.2, y: y + 1.25, w: 5.6, h: 0.35, rectRadius: 0.06, fill: { color: DARKNAVY }, line: { color: DARKNAVY, width: 0 } });
    s.addText(p.code, { x: x + 0.35, y: y + 1.25, w: 5.3, h: 0.35, fontSize: 10, color: CANARY, fontFace: FONTS.mono, valign: 'middle' });
  });

  // Bottom principle / defense banner
  s.addShape(pres.ShapeType.roundRect, { x: 0.45, y: 6.5, w: 12.4, h: 0.55, rectRadius: 0.06, fill: { color: CANARY }, line: { color: INK, width: 2.5 } });
  s.addText('NOVA PRINCIPLE  ·  Maximize memory locality when queues are balanced; dynamically transition to load-balancing when backlog threatens idle stalls.', { x: 0.7, y: 6.55, w: 11.9, h: 0.45, fontSize: 13, bold: true, color: INK, align: 'center', valign: 'middle', fontFace: FONTS.body });
  footer(s, 'NOVA / ALLoC Scheduler Architecture · High-Performance Computing', 5);
}

pres.writeFile({ fileName: './output/HPC_Locality_Scheduling_Presentation.pptx' })
  .then(() => console.log('Wrote 5-slide PPTX'))
  .catch((e) => console.error(e));
