// stitches.config.ts
import { createStitches } from '@stitches/react';

export const {
  styled,
  css,
  globalCss,
  keyframes,
  getCssText,
  theme,
  createTheme,
  config,
} = createStitches({
  theme: {
    colors: {
      good: '#00ff00',
      bad: '#ff0000',
      primary: '#fafafb',
      primaryDimmed: '#aaaaab',
      brand: '#3acecd',
      brandFaded: 'rgba(58, 256, 255, 0.4)',
      gray400: 'gainsboro',
      gray500: '#888888',
      gray600: '#555555',
      gray700: '#3E3E3F',
      gray800: '#2A2A2A',
      gray900: '#151515',
    },
  },
  media: {
    hbp0: '(max-height: 770px)',
    bp0: '(max-width: 479px)',
    bp1: '(min-width: 480px)',
  },
  utils: {
    marginX: (value: number) => ({ marginLeft: value, marginRight: value }),
  },
});

export const globalStyles = globalCss({
  '*': { margin: 0, padding: 0 },
  body: {
    fontFamily: 'Amiko',
    color: '$primary',
    fontSize: 20,
    overflow: 'hidden',
    userSelect: 'none',
  },
  a: {
    textDecoration: 'none',
    textTransform: 'uppercase',
    color: '$primary',
    fontSize: 14,
    letterSpacing: '0.16em',
  },
  'a:hover': {
    fontSize: 14,
    textDecoration: 'none',
    color: '$primaryDimmed',
  },
  th: {
    backgroundColor: '#2A2A2A',
    padding: 4,
    position: 'sticky',
    top: -2,
  },
  td: {
    padding: 4,
    paddingLeft: 0,
    '@bp0': {
      padding: 8,
      fontSize: 24,
    },
  },
  '.flex': {
    display: 'flex',
    width: '100%',
    height: '100%',
  },
  '.flex-col': {
    flexDirection: 'column',
  },
});
