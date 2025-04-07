// Copyright 2025, The ImageInternals Contributors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <commdlg.h>

#include <expected>
#include <fstream>
#include <vector>
#include <span>

#include <maxGUI/maxGUI.hpp>

#include <png.h>

namespace {
	HBITMAP image = (HBITMAP)INVALID_HANDLE_VALUE;
	int width;
	int height;
	HWND window_handle;
	maxGUI::FormConcept* form_ = nullptr;
	std::vector<char> file_contents;
	std::vector<size_t> chunk_indices;
	bool show_picture = true;
	maxGUI::Frame* ihdr_frame = nullptr;
	maxGUI::Label* width_label = nullptr;
	maxGUI::TextBox<>* width_textbox = nullptr;
	maxGUI::Label* height_label = nullptr;
	maxGUI::TextBox<>* height_textbox = nullptr;
	maxGUI::Label* bit_depth_label = nullptr;
	maxGUI::TextBox<>* bit_depth_textbox = nullptr;
	maxGUI::Label* color_type_label = nullptr;
	maxGUI::TextBox<>* color_type_textbox = nullptr;
	maxGUI::Label* compression_method_label = nullptr;
	maxGUI::TextBox<>* compression_method_textbox = nullptr;
	maxGUI::Label* filter_method_label = nullptr;
	maxGUI::TextBox<>* filter_method_textbox = nullptr;
	maxGUI::Label* interlace_method_label = nullptr;
	maxGUI::TextBox<>* interlace_method_textbox = nullptr;


	void DisplaySelectedChunk(size_t chunk_index) noexcept;

	void HideEverything() noexcept {
		show_picture = false;
		ShowWindow(ihdr_frame->window_handle_, SW_HIDE);
		ShowWindow(width_label->window_handle_, SW_HIDE);
		ShowWindow(width_textbox->window_handle_, SW_HIDE);
		ShowWindow(height_label->window_handle_, SW_HIDE);
		ShowWindow(height_textbox->window_handle_, SW_HIDE);
		ShowWindow(bit_depth_label->window_handle_, SW_HIDE);
		ShowWindow(bit_depth_textbox->window_handle_, SW_HIDE);
		ShowWindow(color_type_label->window_handle_, SW_HIDE);
		ShowWindow(color_type_textbox->window_handle_, SW_HIDE);
		ShowWindow(compression_method_label->window_handle_, SW_HIDE);
		ShowWindow(compression_method_textbox->window_handle_, SW_HIDE);
		ShowWindow(filter_method_label->window_handle_, SW_HIDE);
		ShowWindow(filter_method_textbox->window_handle_, SW_HIDE);
		ShowWindow(interlace_method_label->window_handle_, SW_HIDE);
		ShowWindow(interlace_method_textbox->window_handle_, SW_HIDE);
	}

	struct PNGChunksListboxBehavior {


		static void OnSelectionChanged(int newly_selected_index) noexcept {
			// hide everything first
			HideEverything();

			// then selectively show the right thing
			if (newly_selected_index == 0) {
				show_picture = true;
				InvalidateRect(window_handle, nullptr, TRUE);
				return;
			}

			// Subtract 1 because the first selection is the picture
			size_t chunk_index = chunk_indices[newly_selected_index - 1];
			DisplaySelectedChunk(chunk_index);
			InvalidateRect(window_handle, nullptr, TRUE);
		}
	};

	maxGUI::ListBox<PNGChunksListboxBehavior>* listbox_ = nullptr;


	static const std::string_view png_header = "\x89PNG\x0d\x0a\x1a\x0a";

	constexpr uint32_t read_32_bits(const std::span<char>& file_contents, size_t index) noexcept {
		uint8_t first_byte  = file_contents[index + 0];
		uint8_t second_byte = file_contents[index + 1];
		uint8_t third_byte  = file_contents[index + 2];
		uint8_t fourth_byte = file_contents[index + 3];

		// PNGs are little endian
		// TODO: Make sure the target device is also little endian for this to work
		uint32_t combined_bytes = first_byte  << 24 |
			second_byte << 16 |
			third_byte  << 8  |
			fourth_byte << 0;
		return combined_bytes;
	}

	enum class GetChunkIndicesErrorCode {
		NotAPNGFile,
	};
	std::expected<std::vector<size_t>, GetChunkIndicesErrorCode> get_chunk_indices(const std::span<char>& file_contents) noexcept {
		if (png_header.compare(file_contents.data()) != 0) {
			return std::unexpected{ GetChunkIndicesErrorCode::NotAPNGFile };
		}

		std::vector<size_t> chunk_indices;
		size_t current_index = 8; // 8 comes from the the PNG header
		while (current_index < file_contents.size()) {
			chunk_indices.push_back(current_index);


			uint32_t chunk_length = read_32_bits(file_contents, current_index);
			// TODO: Check that chunk_length + 12 doesn't overflow
			current_index += chunk_length + 12; // 12 comes from the chunk length, type, and crc data
		}

		return chunk_indices;
	}

	enum class ReadFileErrorCode {
		CannotOpenFile,
		CannotReadFile,
	};
	std::expected<std::vector<char>, ReadFileErrorCode> read_file(const std::string& file_path) noexcept {
		auto file = std::ifstream{ file_path.c_str(), std::ios::binary | std::ios::ate };
		if (!file.good()) {
			return std::unexpected{ ReadFileErrorCode::CannotOpenFile };
		}
		auto file_size = file.tellg();
		file.seekg(0, std::ios::beg);

		auto buffer = std::vector<char>( file_size );
		if (!file.read(buffer.data(), file_size))
		{
			return std::unexpected{ ReadFileErrorCode::CannotReadFile };
		}

		file.close();

		return buffer;
	}


	struct IHDRContent {
		uint32_t width_;
		uint32_t height_;
		uint8_t bit_depth_;
		uint8_t color_type_;
		/*
		* 0 - Grayscale (1, 2, 4, 8, 16 bits)
		* 2 - Truecolor (8, 16 bits)
		* 3 - Indexed-color (1, 2, 4, 8 bits)
		* 4 - Grayscale w/ alpha (8, 16 bits)
		* 6 - Truecolor w/ alpha (8, 16 bits)
		*/
		uint8_t compression_method_;
		uint8_t filter_method_;
		uint8_t interlace_method_;
	};
	IHDRContent ReadIHDR(const std::span<char>& file_contents, const size_t chunk_index) noexcept {
		static constinit auto ihdr_size = uint32_t{ 13 }; // not including chunk header or crc
		if (read_32_bits(file_contents, chunk_index) != ihdr_size) {
			throw;
		}

		auto ihdr = IHDRContent{};
		ihdr.width_ = read_32_bits(file_contents, chunk_index + 8);
		ihdr.height_ = read_32_bits(file_contents, chunk_index + 12);
		ihdr.bit_depth_ = file_contents[chunk_index + 16];
		ihdr.color_type_ = file_contents[chunk_index + 17];
		ihdr.compression_method_ = file_contents[chunk_index + 18];
		ihdr.filter_method_ = file_contents[chunk_index + 19];
		ihdr.interlace_method_ = file_contents[chunk_index + 20];

		return ihdr;
	}

	void DisplayIHDR(size_t chunk_index) noexcept {
		auto ihdr = ReadIHDR(file_contents, chunk_index);

		width_textbox->SetText(std::to_string(ihdr.width_));
		height_textbox->SetText(std::to_string(ihdr.height_));
		bit_depth_textbox->SetText(std::to_string(ihdr.bit_depth_));
		color_type_textbox->SetText(std::to_string(ihdr.color_type_));
		compression_method_textbox->SetText(std::to_string(ihdr.compression_method_));
		filter_method_textbox->SetText(std::to_string(ihdr.filter_method_));
		interlace_method_textbox->SetText(std::to_string(ihdr.interlace_method_));

		ShowWindow(ihdr_frame->window_handle_, SW_SHOW);
		ShowWindow(width_label->window_handle_, SW_SHOW);
		ShowWindow(width_textbox->window_handle_, SW_SHOW);
		ShowWindow(height_label->window_handle_, SW_SHOW);
		ShowWindow(height_textbox->window_handle_, SW_SHOW);
		ShowWindow(bit_depth_label->window_handle_, SW_SHOW);
		ShowWindow(bit_depth_textbox->window_handle_, SW_SHOW);
		ShowWindow(color_type_label->window_handle_, SW_SHOW);
		ShowWindow(color_type_textbox->window_handle_, SW_SHOW);
		ShowWindow(compression_method_label->window_handle_, SW_SHOW);
		ShowWindow(compression_method_textbox->window_handle_, SW_SHOW);
		ShowWindow(filter_method_label->window_handle_, SW_SHOW);
		ShowWindow(filter_method_textbox->window_handle_, SW_SHOW);
		ShowWindow(interlace_method_label->window_handle_, SW_SHOW);
		ShowWindow(interlace_method_textbox->window_handle_, SW_SHOW);
	}

	void DisplaySelectedChunk(size_t chunk_index) noexcept {
		char first_char  = file_contents[chunk_index + 4];
		char second_char = file_contents[chunk_index + 5];
		char third_char  = file_contents[chunk_index + 6];
		char fourth_char = file_contents[chunk_index + 7];

		if (first_char  == 'I' &&
		    second_char == 'H' &&
		    third_char  == 'D' &&
		    fourth_char == 'R') {
			DisplayIHDR(chunk_index);
		}
	}

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

		auto read_file_result = read_file(utf8_string);
		if (!read_file_result.has_value()) {
			/*
			switch (read_file_result.error()) {
			case ReadFileErrorCode::CannotOpenFile:
			case ReadFileErrorCode::CannotReadFile:
			}
			*/
			return;
		}

		file_contents = std::move(read_file_result.value());

		auto get_chunk_indices_result = get_chunk_indices(file_contents);
		if (!get_chunk_indices_result.has_value()) {
			/*
			switch (get_chunk_indices_result.error()) {
			case GetChunkIndicesErrorCode::NotAPNGFile:
			}
			*/
			return;
		}

		listbox_->Clear();
		listbox_->AddItem("Image");
		chunk_indices = std::move(get_chunk_indices_result.value());
		for (const auto& chunk_index : chunk_indices) {
			char first_char  = file_contents[chunk_index + 4];
			char second_char = file_contents[chunk_index + 5];
			char third_char  = file_contents[chunk_index + 6];
			char fourth_char = file_contents[chunk_index + 7];
			/*auto text = std::string{1, first_char};
			text += second_char;
			text += third_char;
			text += fourth_char;
			*/
			auto text = std::string{first_char, second_char, third_char, fourth_char};
			listbox_->AddItem(text);
		}

		ReadPNG(utf8_string.c_str());

		HideEverything();
		show_picture = true;

		InvalidateRect(window_handle, nullptr, TRUE);
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
	static const int initial_form_width = 800;
	static const int initial_form_height = 600;
	static const int component_width = 200;
	int form_height;
	int form_width;

	void OnCreated(maxGUI::FormConcept* form) noexcept {
		form_ = form;
		window_handle = form->window_handle_;

		std::vector<std::string> listbox_options{};
		listbox_ = form->AddControl<maxGUI::ListBox<PNGChunksListboxBehavior>>(max::Containers::MakeRectangle(initial_form_width - component_width, 275, 200, 150), std::move(listbox_options));

		// IHDR content
		ihdr_frame                 = form->AddControl<maxGUI::Frame>(max::Containers::MakeRectangle(25, 25, initial_form_width - component_width, initial_form_height), "IHDR");
		width_label                = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50,  50, 125, 25), "Width:");
		width_textbox              = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200,  50,  50, 25), "");
		height_label               = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50,  75, 125, 25), "Height:");
		height_textbox             = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200,  75,  50, 25), "");
		bit_depth_label            = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50, 100, 125, 25), "Bit depth:");
		bit_depth_textbox          = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200, 100,  50, 25), "");
		color_type_label           = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50, 125, 125, 25), "Color type:");
		color_type_textbox         = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200, 125,  50, 25), "");
		compression_method_label   = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50, 150, 125, 25), "Compression method:");
		compression_method_textbox = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200, 150,  50, 25), "");
		filter_method_label        = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50, 175, 125, 25), "Filter method:");
		filter_method_textbox      = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200, 175,  50, 25), "");
		interlace_method_label     = form->AddControl<maxGUI::Label>    (max::Containers::MakeRectangle( 50, 200, 125, 25), "Interlace method:");
		interlace_method_textbox   = form->AddControl<maxGUI::TextBox<>>(max::Containers::MakeRectangle(200, 200,  50, 25), "");

		ShowWindow(ihdr_frame->window_handle_, SW_HIDE);
		ShowWindow(width_label->window_handle_, SW_HIDE);
		ShowWindow(width_textbox->window_handle_, SW_HIDE);
		ShowWindow(height_label->window_handle_, SW_HIDE);
		ShowWindow(height_textbox->window_handle_, SW_HIDE);
		ShowWindow(bit_depth_label->window_handle_, SW_HIDE);
		ShowWindow(bit_depth_textbox->window_handle_, SW_HIDE);
		ShowWindow(color_type_label->window_handle_, SW_HIDE);
		ShowWindow(color_type_textbox->window_handle_, SW_HIDE);
		ShowWindow(compression_method_label->window_handle_, SW_HIDE);
		ShowWindow(compression_method_textbox->window_handle_, SW_HIDE);
		ShowWindow(filter_method_label->window_handle_, SW_HIDE);
		ShowWindow(filter_method_textbox->window_handle_, SW_HIDE);
		ShowWindow(interlace_method_label->window_handle_, SW_HIDE);
		ShowWindow(interlace_method_textbox->window_handle_, SW_HIDE);


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

			if (show_picture && image != (HBITMAP)INVALID_HANDLE_VALUE) {
				HDC memory_dc = CreateCompatibleDC(device_context);
				HGDIOBJ old_bitmap_handle = SelectObject(memory_dc, image);

				int top = (form_height - height) / 2;
				if (top < 0) {
					top = 0;
				}
				int left = ((form_width - component_width) - width) / 2;
				if (left < 0) {
					left = 0;
				}
				BitBlt(device_context, left, top, width, height, memory_dc, 0, 0, SRCCOPY);
				SelectObject(device_context, old_bitmap_handle);
				DeleteObject(memory_dc);
			}

			EndPaint(form->window_handle_, &paint_info);
		}
		return 0;
		}

		return DefWindowProc(form->window_handle_, message, wparam, lparam);
	}

	void OnResized(maxGUI::FormConcept* /*form*/, int new_width, int new_height) noexcept {
		form_width = new_width;
		form_height = new_height;
		// Repaint the image in the new center
		InvalidateRect(window_handle, nullptr, FALSE);

		listbox_->Move(max::Containers::MakeRectangle(new_width - component_width, 0, component_width, new_height));
		int ihdr_frame_width = new_width - component_width - 50; // 50 for 25 padding * 2
		if (ihdr_frame_width < 0) {
			ihdr_frame_width = 0;
		}
		int ihdr_frame_height = new_height - 50;
		if (ihdr_frame_height < 0) {
			ihdr_frame_height = 0;
		}
		ihdr_frame->Move(max::Containers::MakeRectangle(25, 25, ihdr_frame_width, ihdr_frame_height));
	}


	void OnClosed(maxGUI::FormConcept* /*form*/) noexcept {
		DeleteObject(image);
		maxGUI::PostExitMessage(0);
	}

	//maxGUI::ListBox<PNGChunksListboxBehavior>* listbox_ = nullptr;
};


int maxGUIEntryPoint(maxGUI::FormContainer form_container) noexcept {
	auto form_allocator = maxGUI::GetDefaultFormAllocator<MainForm>();
	if (!form_container.CreateForm<MainForm>(MainForm::initial_form_width, MainForm::initial_form_height, "ImageInternals", form_allocator.get())) {
		return -1;
	}

	return maxGUI::MessagePump(form_container);
}