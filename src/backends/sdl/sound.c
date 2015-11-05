/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 * Copyright (C) 2010, 2013 Yamagi Burmeister
 * Copyright (C) 2005 Ryan C. Gordon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * =======================================================================
 */

/* Local includes */
#include "../../client/header/client.h"
#include "../../client/sound/header/local.h"

/* Globals */
int *snd_p;
static sound_t *backend;
static int beginofs;
static int playpos = 0;
static int samplesize = 0;
static int snd_inited = 0;
static int snd_scaletable[32][256];
static int snd_vol;
static int soundtime;

/* ------------------------------------------------------------------ */

/* =============================== */
/* Low-pass filter */
/* Based on OpenAL implementation. */

/* Filter's context */
typedef struct {
    float a;
    float gain_hf;
    portable_samplepair_t history[2];
    qboolean is_history_initialized;
} LpfContext;

static const int lpf_reference_frequency = 5000;
static const float lpf_default_gain_hf = 0.25F;

static LpfContext lpf_context;
static qboolean lpf_is_enabled;

static void lpf_initialize(
    LpfContext* lpf_context,
    float gain_hf,
    int target_frequency)
{
    assert(target_frequency > 0);
    assert(lpf_context);

    float g;
    float cw;
    float a;

    const float k_2_pi = 6.283185307F;

    g = gain_hf;

    if (g < 0.01F)
        g = 0.01F;
    else if (gain_hf > 1.0F)
        g = 1.0F;

    cw = cosf((k_2_pi * lpf_reference_frequency) / target_frequency);

    a = 0.0F;

    if (g < 0.9999F) {
        a = (1.0F - (g * cw) - sqrtf((2.0F * g * (1.0F - cw)) -
            (g * g * (1.0F - (cw * cw))))) / (1.0F - g);
    }

    lpf_context->a = a;
    lpf_context->gain_hf = gain_hf;
    lpf_context->is_history_initialized = false;
}

static void lpf_update_samples(
    LpfContext* lpf_context,
    int sample_count,
    portable_samplepair_t* samples)
{
    assert(lpf_context);
    assert(sample_count >= 0);
    assert(samples);

    int s;
    float a;
    portable_samplepair_t y;
    portable_samplepair_t* history;

    if (sample_count <= 0)
        return;

    a = lpf_context->a;
    history = lpf_context->history;

    if (!lpf_context->is_history_initialized) {
        lpf_context->is_history_initialized = true;

        for (s = 0; s < 2; ++s) {
            history[s].left = 0;
            history[s].right = 0;
        }
    }

    for (s = 0; s < sample_count; ++s) {
        /* Update left channel */

        y.left = samples[s].left;

        y.left = (int)(y.left + a * (history[0].left - y.left));
        history[0].left = y.left;

        y.left = (int)(y.left + a * (history[1].left - y.left));
        history[1].left = y.left;


        /* Update right channel */

        y.right = samples[s].right;

        y.right = (int)(y.right + a * (history[0].right - y.right));
        history[0].right = y.right;

        y.right = (int)(y.right + a * (history[1].right - y.right));
        history[1].right = y.right;


        /* Update sample */
        samples[s] = y;
    }
}

/* End of low-pass filter stuff */
/* ============================ */
