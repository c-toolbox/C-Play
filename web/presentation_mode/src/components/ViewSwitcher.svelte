<script lang="ts" module>
  // The web views of C-Play, in each of them (web/*/src/components/ViewSwitcher.svelte, keep the
  // copies the same). The paths are where the HTTP server serves them.
  export type ViewPath = 'views' | 'presenter' | 'movie' | 'layerview';

  export const webViews: { path: ViewPath; name: string; description: string }[] = [
    { path: 'presenter', name: 'Presenter', description: 'Slides, notes and timer' },
    { path: 'movie', name: 'Movie', description: 'Playlist, sections and cues' },
    { path: 'layerview', name: 'Layers', description: 'Position, size, volume and visibility' },
  ];

  /** The address of a view; opened in the C-Play window, it stays in it (host=cplay). */
  export function viewHref(path: ViewPath) {
    const inCPlay = new URLSearchParams(location.search).get('host') === 'cplay';
    return `/${path}/${inCPlay ? '?host=cplay' : ''}`;
  }

  export interface Cluster {
    name: string;
    url: string;
  }

  /** This C-Play cluster and the others to go to, from data/http-server-conf.json. */
  export const clusters = $state<{ name: string; others: Cluster[] }>({ name: '', others: [] });

  let clustersLoaded = false;

  export function loadClusters() {
    if (clustersLoaded) return;
    clustersLoaded = true;
    fetch('/clusters', { cache: 'no-store' })
      .then((res) => (res.ok ? res.json() : null))
      .then((data) => {
        if (!data) return;
        clusters.name = typeof data.name === 'string' ? data.name : '';
        clusters.others = Array.isArray(data.clusters) ? data.clusters.filter((c: Cluster) => clusterHref(c.url)) : [];
      })
      .catch(() => {});
  }

  /** The start page of another cluster; opened in the C-Play window, it stays in it. */
  export function clusterHref(url: string) {
    try {
      const target = new URL(url);
      if (target.protocol !== 'http:' && target.protocol !== 'https:') return '';
      if (target.pathname === '/') target.pathname = '/views/';
      if (new URLSearchParams(location.search).get('host') === 'cplay') target.searchParams.set('host', 'cplay');
      return target.href;
    } catch {
      return '';
    }
  }
</script>

<script lang="ts">
  interface Props {
    /** The view this is shown in. */
    current: ViewPath;
  }

  let { current }: Props = $props();

  let open = $state(false);
  let root = $state<HTMLElement>();

  const currentName = $derived(webViews.find((view) => view.path === current)?.name ?? 'Views');

  loadClusters();

  // Closes on a click elsewhere and on Escape, before the view sees the key.
  $effect(() => {
    if (!open) return;
    const onpointerdown = (event: PointerEvent) => {
      if (!root?.contains(event.target as Node)) open = false;
    };
    const onkeydown = (event: KeyboardEvent) => {
      if (event.key !== 'Escape') return;
      event.preventDefault();
      event.stopPropagation();
      open = false;
    };
    window.addEventListener('pointerdown', onpointerdown, true);
    window.addEventListener('keydown', onkeydown, true);
    return () => {
      window.removeEventListener('pointerdown', onpointerdown, true);
      window.removeEventListener('keydown', onkeydown, true);
    };
  });
</script>

<div class="switcher" bind:this={root}>
  <button type="button" class="ghost toggle" aria-haspopup="menu" aria-expanded={open} title="Go to another C-Play view" onclick={() => (open = !open)}>
    <img src="./favicon.png" alt="" class="logo" />
    <span class="current">{currentName}</span>
    <span class="caret" aria-hidden="true">▾</span>
  </button>
  {#if open}
    <nav class="menu" aria-label="C-Play views">
      {#if clusters.name}<span class="heading">{clusters.name}</span>{/if}
      <a href={viewHref('views')} class:active={current === 'views'} aria-current={current === 'views' ? 'page' : undefined}>
        <span class="name">All views</span>
        <span class="description">The start page with every view</span>
      </a>
      {#each webViews as view (view.path)}
        <a href={viewHref(view.path)} class:active={current === view.path} aria-current={current === view.path ? 'page' : undefined}>
          <span class="name">{view.name}</span>
          <span class="description">{view.description}</span>
        </a>
      {/each}
      {#if clusters.others.length}
        <span class="heading">Other clusters</span>
        {#each clusters.others as cluster (cluster.url)}
          <a href={clusterHref(cluster.url)} class="cluster">
            <span class="name">{cluster.name}</span>
          </a>
        {/each}
      {/if}
    </nav>
  {/if}
</div>

<style>
  .switcher {
    position: relative;
    flex: none;
  }
  .toggle {
    display: flex;
    align-items: center;
    gap: 0.45rem;
    padding: 0.25rem 0.5rem 0.25rem 0.3rem;
  }
  .logo {
    width: 1.6rem;
    height: 1.6rem;
  }
  .current {
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-muted);
  }
  .caret {
    font-size: 0.7rem;
    color: var(--text-muted);
  }
  .menu {
    position: absolute;
    z-index: 50;
    top: calc(100% + 0.3rem);
    left: 0;
    min-width: 16rem;
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
    padding: 0.35rem;
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: var(--radius);
    box-shadow: 0 8px 30px rgb(0 0 0 / 0.35);
  }
  a {
    display: flex;
    flex-direction: column;
    gap: 0.1rem;
    padding: 0.5rem 0.65rem;
    border-radius: 8px;
    color: var(--text);
    text-decoration: none;
    border: 1px solid transparent;
  }
  a:hover {
    background: var(--button-hover);
  }
  a:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 1px;
  }
  a.active {
    border-color: var(--accent);
    background: var(--button);
  }
  .name {
    font-weight: 600;
  }
  .heading {
    padding: 0.4rem 0.65rem 0.15rem;
    font-size: 0.7rem;
    font-weight: 600;
    letter-spacing: 0.05em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .heading:not(:first-child) {
    margin-top: 0.25rem;
    border-top: 1px solid var(--border);
    padding-top: 0.55rem;
  }
  a.cluster {
    padding-block: 0.4rem;
  }
  .description {
    font-size: 0.8rem;
    color: var(--text-muted);
  }
</style>
