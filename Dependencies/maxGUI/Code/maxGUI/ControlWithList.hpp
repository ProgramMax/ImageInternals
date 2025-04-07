// Copyright 2020, The maxGUI Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MAXGUI_CONTROLWITHLIST_HPP
#define MAXGUI_CONTROLWITHLIST_HPP

#include <string>
#include <vector>

#include <max/Compiling/Configuration.hpp>

#include <maxGUI/Control.hpp>

#if defined(MAX_PLATFORM_WINDOWS)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif

	#include <Windows.h>
#endif

namespace {

	// TODO: Use max's Exists here
	template< typename T >
	struct HasOnSelectionChanged {
		typedef char yes[1];
		typedef char no[2];

		// If you want only non-static member functions:
		//template <typename U> static yes& test(typename std::enable_if<std::is_member_function_pointer_v<decltype(&U::OnSelectionChanged)>, bool>::type = 0);
		// If you want static and non-static member functions:
		template <typename U> static yes& test(typename std::enable_if<std::is_function_v<decltype(U::OnSelectionChanged)>, bool>::type = 0);
		template <typename U> static no& test(...);
		static bool const value = sizeof(test<typename std::remove_cv<T>::type>(0)) == sizeof(yes&);
	};

} // anonymous namespace

namespace maxGUI
{

	class ControlWithList : public Control {
	public:

#if defined(MAX_PLATFORM_WINDOWS)
		explicit ControlWithList(HWND window_handle) noexcept;
#endif

		~ControlWithList() noexcept override = default;

	};

} // namespace maxGUI

#endif // #ifndef MAXGUI_CONTROLWITHTEXT_HPP