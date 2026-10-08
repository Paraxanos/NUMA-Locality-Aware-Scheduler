// Reusable slide components matching reference.pdf design language
import PptxGenJS from 'pptxgenjs';
import { COLORS, TYPO, FONTS, SPACING, ROUND_CARD } from './theme';

export function addFooter(slide: PptxGenJS.Slide, label: string, pageNum: number) {
  slide.addText(`NOVA · ALLoC  ·  ${label}`, {
    x: 0.4, y: 7.0, w: 8, h: 0.2,
    fontSize: TYPO.captionSize, color: COLORS.muted, fontFace: FONTS.mono,
  });
  slide.addText(`${String(pageNum).padStart(2, '0')} / 05`, {
    x: 12.4, y: 7.0, w: 0.8, h: 0.2,
    fontSize: TYPO.captionSize, color: COLORS.muted, align: 'right', fontFace: FONTS.mono,
  });
}

export function addPill(slide: PptxGenJS.Slide, text: string, x: number, y: number, w: number = 1.2) {
  slide.addShape(PptxGenJS.ShapeType.roundRect, {
    x, y, w, h: 0.28,
    rectRadius: 0.14,
    fill: { color: COLORS.darkNavy }, line: { color: COLORS.orangeAccent, width: 1 },
  });
  slide.addText(text, {
    x: x + 0.08, y: y - 0.02, w: w - 0.16, h: 0.32,
    fontSize: 8, bold: true, color: COLORS.orangeAccent,
    align: 'center', valign: 'middle', charSpacing: 2,
  });
}

export function addTitle(slide: PptxGenJS.Slide, text: string, x: number = 0.55, y: number = 0.35) {
  slide.addText(text, {
    x, y, w: 12.2, h: 1.0,
    fontSize: TYPO.titleSize, bold: true, color: COLORS.ink,
    fontFace: FONTS.display, align: 'left', valign: 'bottom',
    charSpacing: -2,
  });
}

export function addSubtitle(slide: PptxGenJS.Slide, text: string, x: number = 0.55, y: number = 1.35) {
  slide.addText(text, {
    x, y, w: 12.2, h: 0.5,
    fontSize: TYPO.subtitleSize, color: COLORS.blueAccent,
    fontFace: FONTS.body, bold: true,
  });
}
