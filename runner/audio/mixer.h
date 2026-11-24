/*
 * mixer.h — drain audio event queue and render samples via ym2612 + psg.
 *
 * Called once per wall frame after the machine step returns. Walks the
 * queued (cycle_stamp, port, value) events in order, advancing the chips
 * between events so register writes land at accurate sub-sample cycle
 * positions. Fills caller-provided FM and PSG sample buffers.
 *
 * Phase 1: header stub. Real implementation arrives in Phase 5 switchover.
 */
#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include <stdint.h>
#include <stddef.h>

/* Drain all queued events up to `frame_end_cycle` (68K cycles since
 * wall-frame start). Advances ym2612+psg through events in order and
 * fills the sample buffers for this wall frame.
 *
 * Returns the sample count written to each (may be less than buffer cap
 * on short frames). */
void audio_mixer_drain(uint32_t frame_end_cycle,
                       int16_t *fm_stereo_out, size_t fm_cap, size_t *fm_written,
                       int16_t *psg_mono_out,  size_t psg_cap, size_t *psg_written);

/* Call once at boot. */
void audio_mixer_init(void);
/* Explicit local audition, applied after canonical synthesis/verification.
 * bits0..5 FM1..6, bit6 DAC, bits7..9 PSG tone1..3, bit10 noise.
 * Full mask is0x7FF. All sequences, channels and canonical filters continue.
 * Call outside a drain; a UI restores this preference with its own state. */
void audio_mixer_set_audition(int enabled,unsigned channels);

/* Presentation preference, default0: bypass the native FM/PSG hardware LPFs.
 * Both canonical filters keep advancing. Changes on the emulation/main thread
 * fade over5ms without resetting chips, clocks or playback. Safe before init;
 * the preference survives chip resets. Separately opted-in enhanced FM retains
 * its own output path; its verifier still receives canonical filtered PCM. */
void audio_mixer_set_filter_bypass(int bypass);
/* Host executor for independent canonical and shadow synthesis after common stable ordering,
 * pairguard and deferred-event ownership. MUST execute each job exactly once
 * and return only after both finish. NULL retains the original serial path.
 * Set only outside a drain; capability/selection stays fixed for the context. */
typedef void (*AudioMixerJob)(void *);
typedef void (*AudioMixerExecutor)(AudioMixerJob,void *,AudioMixerJob,void *,void *);
/* Enabled FM shadow synthesis. Canonical
 * FM/PSG stay on the owner; join precedes verification/substitution. */
void audio_mixer_set_shadow_executor(AudioMixerExecutor executor,void *context);

/* Passive read-only observer invoked on the emulation thread after
 * timestamp ordering and per-chip monotonic clamping for every applied
 * YM2612 (ports 0..3) and PSG (port 4) register write in the frame.
 *
 * At the start of each audio_mixer_drain() frame (before any register events
 * for that frame, even when n_apply == 0), the observer is called once with
 * port == 0xFFu (value=0, cycle_stamp=0, frame_end_cycle) to signal a new
 * drain frame boundary. */
typedef void (*AudioChipWriteObserver)(uint8_t port, uint8_t value,
                                       uint32_t cycle_stamp,
                                       uint32_t frame_end_cycle);
void audio_mixer_set_chip_observer(AudioChipWriteObserver observer);

#endif
