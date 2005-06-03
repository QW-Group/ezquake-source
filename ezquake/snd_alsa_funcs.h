/*
	QF ALSA function list,
        adapted for ZQuake by Matthew T. Atkinson

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2002/4/19

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
*/

#ifndef ALSA_FUNC
#define ALSA_FUNC(ret, func, params)
#define UNDEF_ALSA_FUNC
#endif

ALSA_FUNC (int, snd_pcm_close, (snd_pcm_t *pcm))
ALSA_FUNC (int, snd_pcm_hw_params, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params))
ALSA_FUNC (int, snd_pcm_hw_params_any, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params))
ALSA_FUNC (int, snd_pcm_hw_params_get_buffer_size, (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val))
ALSA_FUNC (int, snd_pcm_hw_params_get_period_size, (const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *frames, int *dir))
ALSA_FUNC (int, snd_pcm_hw_params_set_access, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t access))
ALSA_FUNC (int, snd_pcm_hw_params_set_period_size_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir))
ALSA_FUNC (int, snd_pcm_hw_params_set_rate_near, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir))
ALSA_FUNC (int, snd_pcm_hw_params_set_channels, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val))
ALSA_FUNC (int, snd_pcm_hw_params_set_format, (snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val))
ALSA_FUNC (size_t, snd_pcm_hw_params_sizeof, (void))
ALSA_FUNC (int, snd_pcm_mmap_begin, (snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas, snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames))
ALSA_FUNC (int, snd_pcm_avail_update, (snd_pcm_t *pcm))
ALSA_FUNC (snd_pcm_sframes_t, snd_pcm_mmap_commit, (snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames))
ALSA_FUNC (int, snd_pcm_open, (snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode))
ALSA_FUNC (int, snd_pcm_pause, (snd_pcm_t *pcm, int enable))
ALSA_FUNC (int, snd_pcm_start, (snd_pcm_t *pcm))
ALSA_FUNC (snd_pcm_state_t, snd_pcm_state, (snd_pcm_t *pcm))
ALSA_FUNC (int, snd_pcm_sw_params, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params))
ALSA_FUNC (int, snd_pcm_sw_params_current, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params))
ALSA_FUNC (int, snd_pcm_sw_params_set_start_threshold, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val))
ALSA_FUNC (int, snd_pcm_sw_params_set_stop_threshold, (snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val))
ALSA_FUNC (size_t, snd_pcm_sw_params_sizeof, (void))
ALSA_FUNC (const char *, snd_strerror, (int errnum))

#ifdef UNDEF_ALSA_FUNC
#undef ALSA_FUNC
#undef UNDEF_ALSA_FUNC
#endif
