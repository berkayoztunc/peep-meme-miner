module.exports = {
  content: ['../index.html'],
  theme: {
    extend: {
      fontFamily: { mono: ['ui-monospace', 'SFMono-Regular', 'Menlo', 'monospace'] },
      colors: { pump: { 400: '#4ff2b4', 500: '#1fd99a', 600: '#12b57e' } },
      keyframes: { pop: { '0%': { transform: 'translateY(8px)', opacity: 0 }, '100%': { transform: 'none', opacity: 1 } } },
      animation: { pop: 'pop .18s ease-out' },
    },
  },
  plugins: [],
};
