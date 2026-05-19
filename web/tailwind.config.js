/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: ['./index.html', './src/**/*.{svelte,ts,js}'],
  theme: {
    extend: {
      colors: {
        accent: 'rgb(var(--accent) / <alpha-value>)',
      },
      fontFamily: {
        sans: [
          'JetBrains Mono',
          'IBM Plex Mono',
          'SF Mono',
          'ui-monospace',
          'Cascadia Code',
          'Menlo',
          'Consolas',
          'monospace',
        ],
        mono: [
          'JetBrains Mono',
          'IBM Plex Mono',
          'SF Mono',
          'ui-monospace',
          'Cascadia Code',
          'Menlo',
          'Consolas',
          'monospace',
        ],
      },
    },
  },
  plugins: [],
}
