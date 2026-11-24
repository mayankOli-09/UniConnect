/** @type {import('tailwindcss').Config} */
const v = (n) => `rgb(var(--${n}) / <alpha-value>)`;
const scale = (name, steps) => Object.fromEntries(steps.map((s) => [s, v(`${name}-${s}`)]));
const S = [50, 100, 200, 300, 400, 500, 600, 700, 800, 900];

// Every colour is a CSS variable (see index.css), so light and dark themes swap without dark: variants.
export default {
  content: ['./index.html', './src/**/*.{ts,tsx}'],
  theme: {
    extend: {
      colors: {
        white: v('surface'),
        slate: scale('slate', S),
        indigo: scale('brand', S),
        rose: scale('rose', [50, 100, 500, 600, 700]),
        emerald: scale('emerald', [50, 100, 500, 600, 700]),
        amber: scale('amber', [50, 100, 400, 500, 700]),
        snow: '#ffffff',
        marker: v('marker'),
        rail: v('rail'),
      },
      fontFamily: {
        sans: ['"Public Sans"', 'system-ui', 'sans-serif'],
        display: ['"Bricolage Grotesque"', '"Public Sans"', 'system-ui', 'sans-serif'],
      },
    },
  },
  plugins: [],
};
