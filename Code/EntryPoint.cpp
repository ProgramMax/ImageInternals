// Copyright 2025, The ImageInternals Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <commdlg.h>

#include <maxGUI/maxGUI.hpp>

#include <png.h>

namespace {
	HBITMAP image = (HBITMAP)INVALID_HANDLE_VALUE;
	int width;
	int height;
	HWND window_handle;


	void SetImageFromBuffer(HWND window_handle, uint8_t* buffer, int buffer_size, int width, int height) {
		constexpr int color_planes = 1;
		constexpr int bits_per_pixel = 32;

		HDC device_context = GetDC(window_handle);
		if (device_context == NULL) {
			// error
			return;
		}

		HBITMAP bitmap;
		int scanlines_copied;

		HDC memory_dc = CreateCompatibleDC(device_context);
		if (memory_dc == NULL) {
			// error
			goto cleanup_dc;
		}

		bitmap = CreateCompatibleBitmap(device_context, width, height);
		if (bitmap == NULL) {
			// error
			goto cleanup_memory_dc;
		}

		BITMAPINFO bitmap_info;
		memset(&bitmap_info, 0, sizeof(bitmap_info));
		bitmap_info.bmiHeader.biSize = sizeof(bitmap_info);
		bitmap_info.bmiHeader.biWidth = width;
		bitmap_info.bmiHeader.biHeight = -height;
		bitmap_info.bmiHeader.biPlanes = 1;
		bitmap_info.bmiHeader.biBitCount = 32;
		bitmap_info.bmiHeader.biCompression = BI_RGB;
		bitmap_info.bmiHeader.biSizeImage = buffer_size;
		scanlines_copied = SetDIBits(memory_dc, bitmap, 0, static_cast<UINT>(height), buffer, &bitmap_info, DIB_RGB_COLORS);
		if (scanlines_copied == 0) {
			// error
			goto cleanup_memory_dc;
		}

		image = bitmap;

	cleanup_memory_dc:
		DeleteObject(memory_dc);
	cleanup_dc:
		ReleaseDC(window_handle, device_context);
	}
} // Anonymous namespace

void ReadPNG(char const* file_path) {
	png_image image;
	memset(&image, 0, (sizeof image));
	image.version = PNG_IMAGE_VERSION;

	// TODO: There is a png_image_read_from_ to read from memory
	if (png_image_begin_read_from_file(&image, file_path) == 0)
	{
		return;
	}
	
	//image.format = PNG_FORMAT_RGBA;
	image.format = PNG_FORMAT_FLAG_COLOR + PNG_FORMAT_FLAG_ALPHA + PNG_FORMAT_FLAG_BGR;

	png_bytep buffer;
	buffer = (png_bytep)malloc(PNG_IMAGE_SIZE(image));
	if (buffer == NULL) {
		png_image_free(&image);
		return;
	}
		
	// row_stride should be at least PNG_IMAGE_ROW_STRIDE.
	// However, because we used PNG_IMAGE_SIZE() above we can pass 0.
	if (png_image_finish_read(&image, /*background=*/NULL, buffer, /*row_stride=*/0, /*colormap=*/NULL) == 0) {
		return;
	}


	width = image.width;
	height = image.height;
	SetImageFromBuffer(window_handle, buffer, PNG_IMAGE_SIZE(image), width, height);


	free(buffer);
}

struct OpenMenuBehavior {
	static void OnPressed() noexcept {
		OPENFILENAME ofn;
		TCHAR szFileName[MAX_PATH] = TEXT("");
		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		//ofn.hwndOwner = hwnd;
		ofn.lpstrFilter = TEXT("PNG Files (*.png)\0*.png\0All Files (*.*)\0*.*\0");
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
		ofn.lpstrDefExt = TEXT("png");

		BOOL result = GetOpenFileName(&ofn);
		if (result == 0) {
			return;
		}

		int char_count = 0;
		for ( ; char_count < MAX_PATH; char_count++) {
			if (ofn.lpstrFile[char_count] == '\0') {
				break;
			}
		}
		int utf8_char_count = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, char_count, nullptr, 0, nullptr, nullptr);
		std::string utf8_string(static_cast<size_t>(utf8_char_count), '\0');
		// TODO: Make sure we don't overflow the int cast.
		WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, char_count, &utf8_string[0], utf8_char_count, nullptr, nullptr);

		ReadPNG(utf8_string.c_str());
	}
};

struct SaveMenuBehavior {
	static void OnPressed() noexcept {
		OPENFILENAME ofn;
		TCHAR szFileName[MAX_PATH] = TEXT("");
		ZeroMemory(&ofn, sizeof(ofn));

		ofn.lStructSize = sizeof(ofn);
		//ofn.hwndOwner = hwnd;
		ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
		ofn.lpstrFile = szFileName;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrDefExt = TEXT("txt");

		BOOL result = GetSaveFileName(&ofn);
		if (result == 0) {
			return;
		}

		// do something
	}
};

struct ExitMenuBehavior {
	static void OnPressed() noexcept {
		maxGUI::PostExitMessage(0);
	}
};

struct AboutMenuBehavior {
	static void OnPressed() noexcept {
	}
};

struct MainForm {

	void CreateBuffer(HWND window_handle) {
		// Each scan line in the buffer must be word aligned.
		width = 60;
		height = 60;
		constexpr int color_planes = 1;
		constexpr int bits_per_pixel = 32;
		const int buffer_size = (((width * color_planes * bits_per_pixel + 15) >> 4) << 1) * height;

		//auto buffer = std::make_unique<uint8_t[]>(buffer_size);
		auto buffer = std::vector<uint8_t>(buffer_size);

		// Fill in the buffer
		constexpr int circle_center_y = 30;
		constexpr int circle_center_x = 30;
		constexpr double radius = 30.0;

		for (int y = 0; y < height; y++) {
			int y_distance = abs(circle_center_y - y);

			for (int x = 0; x < width; x++) {
				int x_distance = abs(circle_center_x - x);

				double distance_from_pixel_to_circle_center = sqrt(static_cast<double>(x_distance) * x_distance + static_cast<double>(y_distance) * y_distance);

				size_t i = ((y * width) + x) * 4;
				if (distance_from_pixel_to_circle_center < radius) {
					buffer[i+0] = 0x0; // blue channel
					buffer[i+1] = 0x0; // green channel
					buffer[i+2] = 0xff; // red channel
				} else {
					buffer[i+0] = 0; // blue channel
					buffer[i+1] = 0; // green channel
					buffer[i+2] = 0; // red channel

				}
				buffer[i+3] = 0; // unused channel
			}
		}

		SetImageFromBuffer(window_handle, buffer.data(), buffer_size, width, height);
	}

	void OnCreated(maxGUI::FormConcept* form) noexcept {
		window_handle = form->window_handle_;

		std::vector<std::string> listbox_options{"Item 1", "Item 2", "Item 3"};
		listbox_ = form->AddControl<maxGUI::ListBox<>>(max::Containers::MakeRectangle(25, 275, 300, 150), std::move(listbox_options));

		form->AddControl<maxGUI::Frame>(max::Containers::MakeRectangle(25, 150, 300, 50), "Frame");

		CreateBuffer(form->window_handle_);
		auto file_menu = form->AppendMenu<maxGUI::ParentMenu>("&File");
		file_menu->AppendMenu<maxGUI::PressableMenu<OpenMenuBehavior>>("&Open...");
		file_menu->AppendMenu<maxGUI::PressableMenu<SaveMenuBehavior>>("&Save as...");
		file_menu->AppendMenu<maxGUI::PressableMenu<ExitMenuBehavior>>("E&xit");
		auto help_menu = form->AppendMenuW<maxGUI::ParentMenu>("&Help");
		help_menu->AppendMenuW<maxGUI::PressableMenu<AboutMenuBehavior>>("&About...");
	}

	LRESULT OnWindowMessage(maxGUI::FormConcept* form, UINT message, WPARAM wparam, LPARAM lparam) noexcept {
		switch (message) {
		case WM_PAINT:
		{
			PAINTSTRUCT paint_info;
			HDC device_context = BeginPaint(form->window_handle_, &paint_info);

			HDC memory_dc = CreateCompatibleDC(device_context);
			HGDIOBJ old_bitmap_handle = SelectObject(memory_dc, image);
			BitBlt(device_context, 400, 45, width, height, memory_dc, 0, 0, SRCCOPY);
			SelectObject(device_context, old_bitmap_handle);
			DeleteObject(memory_dc);

			EndPaint(form->window_handle_, &paint_info);
		}
		return 0;
		}

		return DefWindowProc(form->window_handle_, message, wparam, lparam);
	}

	void OnResized(maxGUI::FormConcept* /*form*/, int new_width, int new_height) noexcept {
		listbox_->Move(max::Containers::MakeRectangle(0, 0, 300, new_height));
	}


	void OnClosed(maxGUI::FormConcept* /*form*/) noexcept {
		DeleteObject(image);
		maxGUI::PostExitMessage(0);
	}

	maxGUI::ListBox<>* listbox_ = nullptr;
};


int maxGUIEntryPoint(maxGUI::FormContainer form_container) noexcept {
	auto form_allocator = maxGUI::GetDefaultFormAllocator<MainForm>();
	if (!form_container.CreateForm<MainForm>(800, 600, "ImageInternals", form_allocator.get())) {
		return -1;
	}

	return maxGUI::MessagePump(form_container);
}