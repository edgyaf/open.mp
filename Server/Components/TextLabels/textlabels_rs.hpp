/*
 *  This Source Code Form is subject to the terms of the Mozilla Public License,
 *  v. 2.0. If a copy of the MPL was not distributed with this file, You can
 *  obtain one at http://mozilla.org/MPL/2.0/.
 *
 *  The original code is copyright (c) 2022, open.mp team and contributors.
 */

#pragma once

// C ABI of the Rust implementation in `rust/src/ffi.rs`; keep the two in sync.

#include <cstddef>
#include <cstdint>

extern "C"
{
	struct OmpTextLabelsLabel;

	struct OmpTextLabelsVec3
	{
		float x, y, z;
	};

	struct OmpTextLabelsColour
	{
		uint8_t r, g, b, a;
	};

	OmpTextLabelsLabel* omp_textlabels_label_new(const char* text, size_t textLen, OmpTextLabelsColour colour, OmpTextLabelsVec3 pos, float drawDistance, int32_t virtualWorld, bool testLOS);
	void omp_textlabels_label_free(OmpTextLabelsLabel* label);
	/// The view stays valid until the text is changed or the label freed.
	void omp_textlabels_label_get_text(const OmpTextLabelsLabel* label, const char** outPtr, size_t* outLen);
	void omp_textlabels_label_set_text(OmpTextLabelsLabel* label, const char* text, size_t textLen);
	OmpTextLabelsVec3 omp_textlabels_label_get_position(const OmpTextLabelsLabel* label);
	void omp_textlabels_label_set_position(OmpTextLabelsLabel* label, OmpTextLabelsVec3 pos);
	OmpTextLabelsColour omp_textlabels_label_get_colour(const OmpTextLabelsLabel* label);
	void omp_textlabels_label_set_colour(OmpTextLabelsLabel* label, OmpTextLabelsColour colour);
	float omp_textlabels_label_get_draw_distance(const OmpTextLabelsLabel* label);
	void omp_textlabels_label_set_draw_distance(OmpTextLabelsLabel* label, float drawDistance);
	bool omp_textlabels_label_get_test_los(const OmpTextLabelsLabel* label);
	void omp_textlabels_label_set_test_los(OmpTextLabelsLabel* label, bool testLOS);
	int32_t omp_textlabels_label_get_virtual_world(const OmpTextLabelsLabel* label);
	void omp_textlabels_label_set_virtual_world(OmpTextLabelsLabel* label, int32_t virtualWorld);
	bool omp_textlabels_label_world_matches(const OmpTextLabelsLabel* label, int32_t virtualWorld);

	bool omp_textlabels_should_stream_in(bool hasState, bool visible, OmpTextLabelsVec3 labelPos, OmpTextLabelsVec3 playerPos, float maxDistanceSqr);
}
