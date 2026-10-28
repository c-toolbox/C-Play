<script lang="ts">
  import { untrack } from 'svelte';
  import { ApiError, api, sectionsOf, type MovieState } from '../lib/api';
  import type { TakeMode } from '../lib/cue.svelte';
  import { CueList, cueItem, type CueEntry } from '../lib/cuelist.svelte';
  import { formatTime } from '../lib/time';

  interface Props {
    list: CueList;
    movie: MovieState;
    /** Adds the prepared cue, or what is on air when live. */
    addLabel: string;
    canAdd: boolean;
    /** The standby cue can take the prepared cue (prepare mode). */
    canUpdate: boolean;
    /** GO is not possible now (offline, a transition runs). */
    goDisabled: boolean;
    onadd: () => void;
    onupdate: () => void;
    onselect: (index: number) => void;
    ongo: () => void;
    onback: () => void;
    notify: (text: string, error?: boolean) => void;
  }

  let { list, movie, addLabel, canAdd, canUpdate, goDisabled, onadd, onupdate, onselect, ongo, onback, notify }: Props = $props();

  const thenLabels: Record<TakeMode, string> = { auto: 'As C-Play', play: 'Play', pause: 'Pause' };

  let fileName = $state(untrack(() => list.name));
  let files = $state<{ name: string; modified: string }[] | null>(null);
  let folder = $state('');
  let saving = $state(false);
  let listElement: HTMLElement | undefined = $state();

  const standbyEntry = $derived(list.cues[list.standby]);

  $effect(() => {
    if (!fileName && !list.name && movie.name) fileName = movie.name;
  });

  // Keep the standby cue in view.
  $effect(() => {
    listElement?.querySelector(`[data-cue="${list.standby}"]`)?.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  });

  function describe(entry: CueEntry) {
    const index = cueItem(entry, movie);
    const section = entry.section !== null && entry.section >= 0 && index >= 0 ? sectionsOf(movie, index)[entry.section] : undefined;
    return {
      missing: index < 0,
      number: index,
      title: movie.items[index]?.title ?? entry.file,
      section:
        entry.section === null ? '' : entry.section < 0 ? 'No section' : section ? section.title || `Section ${entry.section + 1}` : `Section ${entry.section + 1}?`,
      time: entry.position === null ? '' : formatTime(entry.position, true),
    };
  }

  const failure = (error: unknown, text: string) => notify(error instanceof ApiError ? `${text}: ${error.message}` : text, true);

  async function toggleOpen() {
    if (files) {
      files = null;
      return;
    }
    try {
      ({ files, folder } = await api.cueLists());
    } catch (error) {
      failure(error, 'Could not list the cue lists');
    }
  }

  const discardChanges = () => !list.edited || !list.cues.length || confirm('Discard the changes to the cue list?');

  async function open(name: string) {
    if (!discardChanges()) return;
    files = null;
    try {
      const cues = CueList.parse(await api.loadCueList(name));
      if (!cues) throw new Error();
      list.replace(cues, name);
      fileName = name;
      notify(`Opened ${name}.cplaycue`);
    } catch (error) {
      failure(error, `Could not open ${name}.cplaycue`);
    }
  }

  async function save() {
    const name = fileName.trim().replace(/\.cplaycue$/i, '');
    if (!name) {
      notify('Name the cue list first', true);
      return;
    }
    saving = true;
    try {
      if (name !== list.name) {
        const existing = (await api.cueLists()).files;
        if (existing.some((file) => file.name.toLowerCase() === name.toLowerCase()) && !confirm(`Replace ${name}.cplaycue?`)) return;
      }
      await api.saveCueList(name, list.toFile(movie.name));
      list.saved(name);
      fileName = name;
      notify(`Saved ${name}.cplaycue`);
    } catch (error) {
      failure(error, 'Could not save the cue list');
    } finally {
      saving = false;
    }
  }

  function newList() {
    if (!discardChanges()) return;
    files = null;
    list.replace([], '');
    fileName = movie.name;
  }
</script>

<section class="cue-list" aria-label="Cue list">
  <header>
    <span class="label">Cue list</span>
    <span class="count">{list.cues.length} {list.cues.length === 1 ? 'cue' : 'cues'}{list.edited ? ' · not saved' : ''}</span>
    <span class="spacer"></span>
    <button type="button" class="back" onclick={onback} disabled={list.standby <= 0} title="Standby on the cue before (Page Up)">◀</button>
    <button
      type="button"
      class="go"
      onclick={ongo}
      disabled={goDisabled || !standbyEntry}
      title={standbyEntry ? `Fire cue ${list.standby + 1}: ${standbyEntry.name || describe(standbyEntry).title} (Page Down, G)` : 'No cue on standby'}
      >GO ▶</button
    >
  </header>

  <div class="file">
    <input
      type="text"
      bind:value={fileName}
      placeholder="Cue list name"
      aria-label="Cue list name"
      onkeydown={(event) => event.key === 'Enter' && void save()}
    />
    <button type="button" onclick={save} disabled={saving} title="Save as name.cplaycue in the cue list location of C-Play (else next to the playlist)">Save</button>
    <button type="button" class:selected={!!files} onclick={toggleOpen} title="Open a cue list saved in the cue list location of C-Play">Open ▾</button>
    <button type="button" class="ghost" onclick={newList} disabled={!list.cues.length && !list.name} title="Start an empty cue list">New</button>
  </div>

  {#if files}
    <ul class="files" aria-label="Saved cue lists" title={folder}>
      {#each files as file (file.name)}
        <li>
          <button type="button" onclick={() => open(file.name)}>
            <span class="file-name">{file.name}</span>
            <span class="meta">{new Date(file.modified).toLocaleString()}</span>
          </button>
        </li>
      {:else}
        <li class="empty">No cue lists in {folder || 'the cue list location'}.</li>
      {/each}
    </ul>
  {/if}

  {#if list.cues.length}
    <ol bind:this={listElement}>
      {#each list.cues as entry, index (entry.id)}
        {@const info = describe(entry)}
        <li data-cue={index}>
          <button
            type="button"
            class="cue"
            class:standby={index === list.standby}
            class:fired={index === list.fired}
            class:missing={info.missing}
            onclick={() => onselect(index)}
            title={info.missing ? `${entry.file} is not in the playlist` : entry.file}
          >
            <span class="number">{index + 1}</span>
            <span class="text">
              <span class="title">{entry.name || info.title}</span>
              <span class="meta">
                {#if entry.name}{info.title} ·{/if}
                {info.section || (entry.position === null ? 'From the start' : '')}{info.section && info.time ? ' · ' : ''}{info.time ? `at ${info.time}` : ''}
                · {thenLabels[entry.then]}
              </span>
            </span>
            <span class="go-mode" class:fade={entry.go === 'fade'}>{entry.go === 'fade' ? 'Fade' : 'Cut'}</span>
            {#if index === list.standby}<span class="badge standby-badge">Next</span>{/if}
            {#if index === list.fired}<span class="badge fired-badge">Fired</span>{/if}
          </button>
          {#if index === list.standby}
            <div class="edit">
              <input
                type="text"
                value={entry.name}
                placeholder={info.title}
                aria-label="Cue name"
                onchange={(event) => list.update(index, { name: (event.currentTarget as HTMLInputElement).value.trim() })}
                onkeydown={(event) => event.key === 'Enter' && (event.currentTarget as HTMLInputElement).blur()}
              />
              <div class="segmented" role="radiogroup" aria-label="Go">
                <button type="button" role="radio" aria-checked={entry.go === 'cut'} class:selected={entry.go === 'cut'} onclick={() => list.update(index, { go: 'cut' })} title="Goes on air at once">Cut</button>
                <button type="button" role="radio" aria-checked={entry.go === 'fade'} class:selected={entry.go === 'fade'} onclick={() => list.update(index, { go: 'fade' })} title="With the transition: fade out, load, fade in">Fade</button>
              </div>
              <select
                value={entry.then}
                aria-label="Then"
                title="After the cue is fired"
                onchange={(event) => list.update(index, { then: (event.currentTarget as HTMLSelectElement).value as TakeMode })}
              >
                {#each Object.entries(thenLabels) as [mode, label] (mode)}
                  <option value={mode}>{label}</option>
                {/each}
              </select>
              <span class="spacer"></span>
              <button type="button" class="ghost" onclick={() => list.move(index, -1)} disabled={index === 0} aria-label="Move up" title="Move up">↑</button>
              <button type="button" class="ghost" onclick={() => list.move(index, 1)} disabled={index === list.cues.length - 1} aria-label="Move down" title="Move down">↓</button>
              <button type="button" class="ghost danger" onclick={() => list.remove(index)} aria-label="Delete cue" title="Delete the cue">✕</button>
            </div>
          {/if}
        </li>
      {/each}
      <li data-cue={list.cues.length}>
        <button type="button" class="cue end" class:standby={list.standby >= list.cues.length} onclick={() => onselect(list.cues.length)}>
          <span class="number"></span>
          <span class="text"><span class="meta">End of the cue list</span></span>
        </button>
      </li>
    </ol>
  {:else}
    <p class="empty">No cues. Prepare a cue (item, section, time) and add it, or add what is on air.</p>
  {/if}

  <div class="actions">
    <button type="button" onclick={onadd} disabled={!canAdd} title="Insert after the cue on standby">+ {addLabel}</button>
    {#if canUpdate && standbyEntry}
      <button type="button" onclick={onupdate} title="Replace the item, section, time and then of the standby cue with the prepared cue">Update cue {list.standby + 1}</button>
    {/if}
  </div>
</section>

<style>
  .cue-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    min-height: 0;
    min-width: 0;
  }
  header {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  .label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .count {
    color: var(--text-muted);
    font-size: 0.85rem;
    white-space: nowrap;
  }
  .spacer {
    flex: 1;
  }
  .go {
    padding: 0.45rem 1.1rem;
    font-weight: 700;
    background: var(--accent);
    border-color: var(--accent);
    color: var(--accent-text);
  }
  .go:hover:not(:disabled) {
    background: var(--accent);
    filter: brightness(1.1);
  }
  .back {
    padding: 0.45rem 0.6rem;
  }
  .file,
  .edit,
  .actions {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 0.35rem;
  }
  .file input {
    flex: 1;
    min-width: 6rem;
  }
  input,
  select {
    font: inherit;
    font-size: 0.9rem;
    padding: 0.4rem 0.55rem;
    border-radius: 8px;
    border: 1px solid var(--border);
    background: var(--bg);
    color: var(--text);
  }
  input:focus,
  select:focus {
    outline: 2px solid var(--next);
    outline-offset: -1px;
  }
  .file button,
  .edit button,
  .actions button {
    padding: 0.35rem 0.7rem;
    font-size: 0.85rem;
  }
  .selected {
    border-color: var(--next);
    box-shadow: inset 0 0 0 1px var(--next);
  }
  .files {
    list-style: none;
    margin: 0;
    padding: 0.3rem;
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    max-height: 12rem;
    overflow-y: auto;
    border: 1px solid var(--border);
    border-radius: var(--radius);
    background: var(--bg);
  }
  .files button {
    width: 100%;
    display: flex;
    justify-content: space-between;
    gap: 0.6rem;
    text-align: left;
  }
  .file-name {
    font-weight: 600;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
  }
  ol {
    list-style: none;
    margin: 0;
    padding: 0.15rem;
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
    overflow-y: auto;
    min-height: 0;
    flex: 1;
    scrollbar-width: thin;
  }
  li[data-cue] {
    display: flex;
    flex-direction: column;
    gap: 0.35rem;
  }
  .cue {
    width: 100%;
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.5rem 0.7rem;
    text-align: left;
    background: var(--surface);
    border: 1px solid var(--border);
    border-left: 4px solid var(--border);
    border-radius: var(--radius);
  }
  .cue.fired {
    border-left-color: var(--accent);
  }
  .cue.standby {
    border-color: var(--next);
    border-left-color: var(--next);
    box-shadow: 0 0 0 1px var(--next);
  }
  .cue.missing .title {
    color: var(--danger);
    text-decoration: line-through;
  }
  .cue.end {
    padding: 0.3rem 0.7rem;
  }
  .number {
    flex: none;
    min-width: 1.4rem;
    font-variant-numeric: tabular-nums;
    color: var(--text-muted);
    text-align: right;
  }
  .text {
    display: flex;
    flex-direction: column;
    gap: 0.1rem;
    min-width: 0;
    flex: 1;
  }
  .title {
    font-weight: 600;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .meta {
    font-size: 0.8rem;
    color: var(--text-muted);
    font-variant-numeric: tabular-nums;
  }
  .go-mode {
    flex: none;
    font-size: 0.72rem;
    color: var(--text-muted);
  }
  .go-mode.fade {
    color: var(--warn);
  }
  .badge {
    flex: none;
    font-size: 0.68rem;
    font-weight: 700;
    letter-spacing: 0.04em;
    text-transform: uppercase;
    padding: 0.12rem 0.45rem;
    border-radius: 999px;
  }
  .standby-badge {
    background: var(--next);
    color: #fff;
  }
  .fired-badge {
    background: var(--accent);
    color: var(--accent-text);
  }
  .edit {
    padding: 0 0.2rem 0.3rem 2.2rem;
  }
  .edit input {
    flex: 1;
    min-width: 7rem;
  }
  .segmented {
    display: flex;
    gap: 0.2rem;
  }
  .danger {
    color: var(--danger);
  }
  .empty {
    margin: 0;
    color: var(--text-muted);
    font-size: 0.9rem;
  }
</style>
