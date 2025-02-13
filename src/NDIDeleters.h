#pragma once

#include <type_traits>
#include <functional>

// Concept to check if T is a pointer
template <typename T>
concept IsPointer = std::is_pointer_v<T>;

// Concept to check if F is a callable (function, lambda, or functor)
template <typename F>
concept IsCallable = std::is_invocable_v<F> || std::is_function_v<std::remove_pointer_t<F>>;

// Simple template class accepting a function to call upon destruction, and a pointer on which to call
//  that function. Many NDISDK objects have a C style free function that must be called on them to release
//  them properly. This guard object gives us an RAII style object to do that for us.
template <IsCallable F, IsPointer P>
struct deleteGuard
{
	F funcToCall;  // Store the callable
	P ptr;   // Store the pointer

	deleteGuard(F deleterFunction, P toDelete)
	{
		funcToCall = deleterFunction;
		ptr = toDelete;
	}
	~deleteGuard()
	{
		if (ptr) { funcToCall(ptr); }
	}
};



