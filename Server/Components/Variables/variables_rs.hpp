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
	struct OmpVariablesStorage;

	OmpVariablesStorage* omp_variables_storage_new();
	void omp_variables_storage_free(OmpVariablesStorage* storage);

	void omp_variables_set_string(OmpVariablesStorage* storage, const char* key, size_t keyLen, const char* value, size_t valueLen);
	bool omp_variables_get_string(const OmpVariablesStorage* storage, const char* key, size_t keyLen, const char** outPtr, size_t* outLen);
	void omp_variables_set_int(OmpVariablesStorage* storage, const char* key, size_t keyLen, int32_t value);
	int32_t omp_variables_get_int(const OmpVariablesStorage* storage, const char* key, size_t keyLen);
	void omp_variables_set_float(OmpVariablesStorage* storage, const char* key, size_t keyLen, float value);
	float omp_variables_get_float(const OmpVariablesStorage* storage, const char* key, size_t keyLen);
	/// Returns a `VariableType`.
	int32_t omp_variables_get_type(const OmpVariablesStorage* storage, const char* key, size_t keyLen);
	bool omp_variables_erase(OmpVariablesStorage* storage, const char* key, size_t keyLen);
	bool omp_variables_key_at(const OmpVariablesStorage* storage, int32_t index, const char** outPtr, size_t* outLen);
	size_t omp_variables_size(const OmpVariablesStorage* storage);
	void omp_variables_clear(OmpVariablesStorage* storage);
}
