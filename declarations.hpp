#pragma once
#pragma warning(disable : 4302)
#pragma warning(disable : 4311)
#pragma warning(disable : 4267)
#define NOMINMAX

#include <Windows.h>
#include <Psapi.h>
#include <stdio.h>
#include <algorithm>
#include <thread>

#define TOGGLE(var) var = !var

// helper to extract 32bit integers from 64bit longs while reading from 64bit address space
template<typename T, typename TT>
constexpr auto get_nbytes(TT val, size_t n)
{
	return *reinterpret_cast<T*>(uintptr_t(&val) + n);
}