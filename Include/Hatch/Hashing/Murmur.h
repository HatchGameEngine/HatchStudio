#pragma once

Hash Murmur_HashData(const void* data, size_t size, Hash base = { 0xDEADBEEF, 0, 0, 0 });
Hash Murmur_HashString(CString text, Hash base = { 0xDEADBEEF, 0, 0, 0 });
