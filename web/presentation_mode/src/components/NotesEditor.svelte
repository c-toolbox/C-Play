<script lang="ts">
  import { untrack, type Snippet } from 'svelte';
  import type { SlideInfo } from '../lib/api';
  import { load, store } from '../lib/storage';

  interface Props {
    slide: SlideInfo | undefined;
    /** Why this slide's notes are shown: "current", "next" or "browsing". */
    role: string;
    onsave: (index: number, notes: string) => Promise<void>;
    heading?: Snippet;
  }

  let { slide, role, onsave, heading }: Props = $props();

  const fontSizes = [16, 20, 24, 30, 36, 44, 54];
  let fontSize = $state<number>(load('notesFontSize', 30));

  // The text being edited. Follows the slide's notes, unless they are being edited here.
  let text = $state('');
  let editedIndex = $state<number | null>(null);
  let focused = $state(false);
  let status = $state<'saved' | 'pending' | 'saving' | 'error'>('saved');
  let timer: ReturnType<typeof setTimeout> | undefined;

  $effect(() => {
    const index = slide?.index ?? null;
    const notes = slide?.notes ?? '';
    if (index !== editedIndex) {
      untrack(() => void flush());
      editedIndex = index;
      text = notes;
      status = 'saved';
    } else if (!focused && status === 'saved') {
      text = notes;
    }
  });

  async function flush() {
    clearTimeout(timer);
    if (status !== 'pending' || editedIndex === null) return;
    const index = editedIndex;
    const notes = text;
    status = 'saving';
    try {
      await onsave(index, notes);
      if (editedIndex === index && text === notes) status = 'saved';
      else if (editedIndex === index) status = 'pending';
    } catch {
      if (editedIndex === index) status = 'error';
    }
  }

  function edited() {
    status = 'pending';
    clearTimeout(timer);
    timer = setTimeout(flush, 600);
  }

  function resize(step: number) {
    const i = fontSizes.findIndex((size) => size >= fontSize);
    fontSize = fontSizes[Math.max(0, Math.min(fontSizes.length - 1, (i < 0 ? fontSizes.length - 1 : i) + step))];
    store('notesFontSize', fontSize);
  }

  const statusText = { saved: 'Saved to slide', pending: 'Editing…', saving: 'Saving…', error: 'Not saved, retrying on next edit' };
</script>

<section class="notes">
  <header>
    <span class="label">Notes</span>
    <span class="target">{role}</span>
    <span class="spacer"></span>
    <span class="status" class:error={status === 'error'}>{slide ? statusText[status] : ''}</span>
    <div class="sizes" role="group" aria-label="Notes text size">
      <button type="button" class="ghost" onclick={() => resize(-1)} title="Smaller text" aria-label="Smaller text">A−</button>
      <button type="button" class="ghost" onclick={() => resize(1)} title="Larger text" aria-label="Larger text">A+</button>
    </div>
  </header>
  {#if heading}<div class="extra">{@render heading()}</div>{/if}
  <textarea
    bind:value={text}
    oninput={edited}
    onfocus={() => (focused = true)}
    onblur={() => {
      focused = false;
      void flush();
    }}
    onkeydown={(event) => event.key === 'Escape' && (event.currentTarget as HTMLTextAreaElement).blur()}
    disabled={!slide}
    placeholder={slide ? `Notes for “${slide.name}”…` : 'No slide'}
    spellcheck="true"
    style:font-size="{fontSize}px"
  ></textarea>
</section>

<style>
  .notes {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    min-height: 0;
    min-width: 0;
  }
  header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    min-width: 0;
    flex-wrap: wrap;
  }
  .extra {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    flex-wrap: wrap;
  }
  .extra:empty {
    display: none;
  }
  .label {
    font-size: 0.75rem;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: var(--text-muted);
  }
  .target {
    color: var(--text-muted);
  }
  .spacer {
    flex: 1;
  }
  .status {
    font-size: 0.8rem;
    color: var(--text-muted);
  }
  .status.error {
    color: var(--danger);
  }
  .sizes {
    display: flex;
    gap: 0.25rem;
  }
  textarea {
    flex: 1;
    min-height: 8rem;
    resize: none;
    width: 100%;
    padding: 0.9rem 1rem;
    line-height: 1.35;
    border-radius: var(--radius);
    border: 1px solid var(--border);
    background: var(--surface);
    color: var(--text);
    font-family: inherit;
  }
  textarea:focus {
    outline: 2px solid var(--accent);
    outline-offset: -1px;
  }
  textarea:disabled {
    opacity: 0.5;
  }
</style>
