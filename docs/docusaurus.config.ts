import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

// This runs in Node.js - Don't use client-side code here (browser APIs, JSX...)

const config: Config = {
  title: 'C-Play Documentation',
  tagline: 'Open source media player for clustered environments',
  favicon: 'img/favicon.ico',

  // Future flags, see https://docusaurus.io/docs/api/docusaurus-config#future
  future: {
    v4: true, // Improve compatibility with the upcoming Docusaurus v4
  },

  // GitHub Actions (.github/workflows/pages.yml) can override both via
  // DOCS_URL / DOCS_BASE_URL if the site is served from a different origin or
  // path. The defaults below match the current GitHub Pages project URL.
  url: process.env.DOCS_URL || 'https://c-toolbox.github.io',
  baseUrl: process.env.DOCS_BASE_URL || '/C-Play/',
  trailingSlash: true,

  organizationName: 'c-toolbox',
  projectName: 'C-Play',

  onBrokenLinks: 'throw',

  markdown: {
    // future.v4 would otherwise parse .md as plain CommonMark, disabling admonitions.
    format: 'mdx',
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          // Serve the docs at the site root, so pages live at /install/ etc.
          // instead of /docs/install/. The top-level index.md (Home) is the
          // landing page at /.
          routeBasePath: '/',
          sidebarPath: './sidebars.ts',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  plugins: [
    [
      '@cmfcmf/docusaurus-search-local',
      {
        indexDocs: true, // docs are the main content of this site
        indexBlog: false, // blog is disabled in the classic preset above
        indexPages: false, // no standalone pages; the docs serve the whole site
        language: 'en',
      },
    ],
  ],

  themeConfig: {
    image: 'img/logo.png',
    colorMode: {
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: 'C-Play',
      logo: {
        alt: 'C-Play',
        src: 'img/logo.png',
      },
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'docsSidebar',
          position: 'left',
          label: 'Documentation',
        },
        {to: '/setup', label: 'Setup', position: 'left'},
        {to: '/media', label: 'Media', position: 'left'},
        {to: '/playback', label: 'Playback', position: 'left'},
        {to: '/remote-control', label: 'Remote control', position: 'left'},
        {
          href: 'https://github.com/c-toolbox/C-Play',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Getting started',
          items: [
            {label: 'Home', to: '/'},
            {label: 'Install', to: '/install'},
            {label: 'Setup', to: '/setup'},
          ],
        },
        {
          title: 'Guides',
          items: [
            {label: 'Media file structure', to: '/media'},
            {label: 'Settings', to: '/settings'},
            {label: 'Playback features', to: '/playback'},
            {label: 'Remote control', to: '/remote-control'},
            {label: 'System integration', to: '/system-integration'},
          ],
        },
        {
          title: 'Development',
          items: [
            {label: 'Build from code', to: '/build'},
            {label: 'Versions', to: '/versions'},
          ],
        },
      ],
      copyright: `Copyright © 2021-${new Date().getFullYear()} Erik Sundén. Distributed under a GNU General Public License v3.0.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
