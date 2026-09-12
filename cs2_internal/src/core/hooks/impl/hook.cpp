#include <core/common.hpp>
#include <core/memory.hpp>
#include <core/hooks/hook.hpp>
#include <core/security/security.cpp>
#include <core/settings.hpp>
#include <core/menu/rendering.hpp>
#include <data/particles/effects.hpp>
#include <data/particles/weather.hpp>
#include <data/particles/custom.hpp>
#include <core/features.hpp>
#include <modules/economy/economy.h>
#include <modules/visuals/misc/cs2_internal_sb_logo.h>

#include <valve/classes/CSchemaSystem.h>
#include <valve/schemas/CBaseEntity.h>
#include <valve/schemas/CBasePlayerController.h>
#include <valve/schemas/CBasePlayerPawn.h>
#include <valve/schemas/CCollisionProperty.h>
#include <valve/schemas/CCSGameRules.h>
#include <valve/schemas/CEconEntity.h>
#include <valve/schemas/CGameSceneNode.h>
#include <valve/schemas/CGrenade.h>
#include <valve/schemas/CPlantedC4.h>
#include <valve/schemas/CWeapon.h>

#include <modules/visuals/particles/manager.h>
#include <modules/visuals/particles/modulation.h>
#include <modules/visuals/particles/ground.h>
#include <modules/visuals/particles/hitkill.h>
#include <modules/visuals/particles/throwable_trail.h>
#include <modules/visuals/skinchanger/custom_paint.h>
#include <modules/visuals/esp/primitive_buffer.hpp>

#include <wincodec.h>

#pragma comment( lib, "windowscodecs.lib" )

namespace hooking::allocator {

	namespace detail {

		static LONG nt_allocate_virtual_memory(HANDLE process, PVOID* base_addr, ULONG_PTR zero_bits, PSIZE_T region_size, ULONG type, ULONG protect) {
			return reinterpret_cast<LONG(__stdcall*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG)>(MODULE_EXPORT("ntdll.dll:NtAllocateVirtualMemory")) (process, base_addr, zero_bits, region_size, type, protect);
		}

		static LONG nt_free_virtual_memory(HANDLE process, PVOID* base_addr, PSIZE_T region_size, ULONG free_type) {
			return reinterpret_cast<LONG(__stdcall*)(HANDLE, PVOID*, PSIZE_T, ULONG)>(MODULE_EXPORT("ntdll.dll:NtFreeVirtualMemory")) (process, base_addr, region_size, free_type);
		}

	}

	void* allocate(std::size_t size, void* near_) {
		if (!size) {
			return nullptr;
		}

		const auto origin = reinterpret_cast<std::uintptr_t>(near_);
		const auto min_addr = origin > 0x7fff0000ull ? origin - 0x7fff0000ull : 0x10000ull;
		const auto max_addr = origin + 0x7fff0000ull;

		auto try_at = [size](std::uintptr_t address) -> void* {
			auto base = reinterpret_cast<void*>(address);
			auto region = size;
			if (detail::nt_allocate_virtual_memory(GetCurrentProcess(), &base, 0, &region, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE) >= 0 && base) {
				return base;
			}
			return nullptr;
		};

		auto try_region = [&](const MEMORY_BASIC_INFORMATION& mbi) -> void* {
			if (mbi.State != MEM_FREE || mbi.RegionSize < size) {
				return nullptr;
			}

			const auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
			const auto region_end = region_base + mbi.RegionSize;
			if (region_end < region_base || region_end - region_base < size) {
				return nullptr;
			}

			auto candidate = origin;
			if (candidate < region_base) {
				candidate = region_base;
			}
			else if (candidate > region_end - size) {
				candidate = region_end - size;
			}

			candidate &= ~0xffffull;
			if (candidate < region_base) {
				candidate = region_base;
			}

			if (candidate >= min_addr && candidate <= max_addr && candidate + size <= region_end) {
				if (const auto p = try_at(candidate)) {
					return p;
				}
			}

			if (region_base >= min_addr && region_base <= max_addr) {
				return try_at(region_base);
			}

			return nullptr;
		};

		MEMORY_BASIC_INFORMATION mbi{};
		const auto origin_page = origin & ~0xffffull;

		for (auto addr = origin_page; addr >= min_addr; ) {
			if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
				break;
			}

			if (const auto p = try_region(mbi)) {
				return p;
			}

			const auto region_base = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
			if (region_base <= min_addr) {
				break;
			}

			addr = region_base - 1;
		}

		for (auto addr = origin_page; addr <= max_addr; ) {
			if (VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi)) == 0) {
				break;
			}

			if (const auto p = try_region(mbi)) {
				return p;
			}

			const auto region_end = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
			if (region_end <= addr) {
				addr += 0x10000;
			}
			else {
				addr = region_end;
			}
		}

		return nullptr;
	}

	void free(void* address) {
		if (!address) {
			return;
		}

		auto base = address;
		auto region{ 0ull };

		detail::nt_free_virtual_memory(GetCurrentProcess(), &base, &region, MEM_RELEASE);
	}

}

namespace hooking {

	namespace detail {

		constexpr auto min_hook_size{ 14ull };

		static bool decode(ZydisDecodedInstruction* ix, std::uint8_t* ip)
		{
			ZydisDecoder decoder{};
			ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
			return ZYAN_SUCCESS(ZydisDecoderDecodeInstruction(&decoder, nullptr, ip, 15, ix));
		}

		static void write_jmp_rel32(void* at, std::intptr_t offset)
		{
			auto* p = static_cast<std::uint8_t*>(at);
			p[0] = 0xe9;
			*reinterpret_cast<std::int32_t*>(p + 1) = static_cast<std::int32_t>(offset);
		}

		static void write_jmp_abs64(void* at, const void* dest)
		{
			auto* p = static_cast<std::uint8_t*>(at);
			p[0] = 0xff;
			p[1] = 0x25;
			p[2] = 0x00;
			p[3] = 0x00;
			p[4] = 0x00;
			p[5] = 0x00;
			*reinterpret_cast<std::uint64_t*>(p + 6) = reinterpret_cast<std::uint64_t>(dest);
		}

		static void write_call_abs64(void* at, const void* dest)
		{
			auto* p = static_cast<std::uint8_t*>(at);
			p[0] = 0xff;
			p[1] = 0x15;
			p[2] = 0x00;
			p[3] = 0x00;
			p[4] = 0x00;
			p[5] = 0x00;
			*reinterpret_cast<std::uint64_t*>(p + 6) = reinterpret_cast<std::uint64_t>(dest);
		}

		static bool fits_i32(std::intptr_t value)
		{
			return value >= INT32_MIN && value <= INT32_MAX;
		}

		static void flush_icode(void* at, std::size_t size)
		{
			if (at && size)
			{
				FlushInstructionCache(GetCurrentProcess(), at, size);
			}
		}

		static std::size_t write_jmp_auto(void* at, const void* src_next, const void* dest)
		{
			const auto rel = reinterpret_cast<std::intptr_t>(dest) - reinterpret_cast<std::intptr_t>(src_next);

			if (fits_i32(rel))
			{
				write_jmp_rel32(at, rel);
				return 5;
			}

			write_jmp_abs64(at, dest);
			return 14;
		}

		static bool build_trampoline(void* target, void* trampoline, std::size_t* out_original_len, std::size_t* out_trampoline_len)
		{
			auto src = static_cast<std::uint8_t*>(target);
			auto dst = static_cast<std::uint8_t*>(trampoline);

			auto src_offset{ 0ull };
			auto dst_offset{ 0ull };
			auto total_copied{ 0ull };

			ZydisDecodedInstruction ix{};

			while (total_copied < min_hook_size)
			{
				if (!decode(&ix, src + src_offset))
				{
					return false;
				}

				const auto instr_len = ix.length;
				auto ip = src + src_offset;
				auto tr = dst + dst_offset;

				if (ix.attributes & ZYDIS_ATTRIB_IS_RELATIVE)
				{
					if (ix.raw.disp.size == 32)
					{
						std::memcpy(tr, ip, instr_len);
						auto target_addr = ip + instr_len + static_cast<std::int32_t>(ix.raw.disp.value);
						const auto new_disp = target_addr - (tr + instr_len);
						if (!fits_i32(new_disp))
						{
							return false;
						}
						*reinterpret_cast<std::int32_t*>(tr + ix.raw.disp.offset) = static_cast<std::int32_t>(new_disp);
						dst_offset += instr_len;
					}
					else if (ix.raw.imm[0].size == 32)
					{
						auto target_addr = ip + instr_len + static_cast<std::int32_t>(ix.raw.imm[0].value.s);
						const auto new_disp = target_addr - (tr + instr_len);
						if (fits_i32(new_disp))
						{
							std::memcpy(tr, ip, instr_len);
							*reinterpret_cast<std::int32_t*>(tr + ix.raw.imm[0].offset) = static_cast<std::int32_t>(new_disp);
							dst_offset += instr_len;
						}
						else if (ix.mnemonic == ZYDIS_MNEMONIC_JMP)
						{
							if (dst_offset + 14 > 48)
							{
								return false;
							}
							write_jmp_abs64(tr, target_addr);
							dst_offset += 14;
						}
						else if (ix.mnemonic == ZYDIS_MNEMONIC_CALL)
						{
							if (dst_offset + 14 > 48)
							{
								return false;
							}
							write_call_abs64(tr, target_addr);
							dst_offset += 14;
						}
						else
						{
							return false;
						}
					}
					else if (ix.meta.category == ZYDIS_CATEGORY_COND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT)
					{
						auto* target_addr = ip + instr_len + static_cast<std::int8_t>(ix.raw.imm[0].value.s);
						const auto new_disp = target_addr - (tr + 6);
						if (!fits_i32(new_disp))
						{
							return false;
						}
						*tr++ = 0x0f;
						*tr++ = static_cast<std::uint8_t>(0x80 + (ix.opcode & 0x0f));
						*reinterpret_cast<std::int32_t*>(tr) = static_cast<std::int32_t>(new_disp);
						dst_offset += 6;
					}
					else if (ix.meta.category == ZYDIS_CATEGORY_UNCOND_BR && ix.meta.branch_type == ZYDIS_BRANCH_TYPE_SHORT)
					{
						auto* target_addr = ip + instr_len + static_cast<std::int8_t>(ix.raw.imm[0].value.s);
						const auto new_disp = target_addr - (tr + 5);
						if (!fits_i32(new_disp))
						{
							write_jmp_abs64(tr, target_addr);
							dst_offset += 14;
						}
						else
						{
							*tr++ = 0xe9;
							*reinterpret_cast<std::int32_t*>(tr) = static_cast<std::int32_t>(new_disp);
							dst_offset += 5;
						}
					}
					else
					{
						return false;
					}
				}
				else
				{
					std::memcpy(tr, ip, instr_len);
					dst_offset += instr_len;
				}

				src_offset += instr_len;
				total_copied += instr_len;

				if (dst_offset > 48)
				{
					return false;
				}
			}

			dst_offset += write_jmp_auto(dst + dst_offset, dst + dst_offset + 5, src + src_offset);

			*out_original_len = src_offset;
			*out_trampoline_len = dst_offset;
			return true;
		}

		static LONG nt_protect_virtual_memory(HANDLE process, PVOID* base, PSIZE_T size, ULONG protect, PULONG old_protect)
		{
			return reinterpret_cast<LONG(__stdcall*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG)>(MODULE_EXPORT("ntdll.dll:NtProtectVirtualMemory"))(process, base, size, protect, old_protect);
		}

		static LONG nt_query_virtual_memory(HANDLE process, PVOID address, int info_class, PVOID buffer, SIZE_T length, PSIZE_T return_length)
		{
			return reinterpret_cast<LONG(__stdcall*)(HANDLE, PVOID, int, PVOID, SIZE_T, PSIZE_T)>(MODULE_EXPORT("ntdll.dll:NtQueryVirtualMemory"))(process, address, info_class, buffer, length, return_length);
		}

	}

	bool jmp::create(void* target, void* hook_function)
	{
		if (this->is_valid())
		{
			return true;
		}

		if (!target || !hook_function)
		{
			return false;
		}

		auto resolved = reinterpret_cast<std::uintptr_t>(target);

		for (auto depth = 0; depth < 8; ++depth)
		{
			MEMORY_BASIC_INFORMATION resolved_mbi{};
			auto resolved_length{ 0ull };
			if (detail::nt_query_virtual_memory(GetCurrentProcess(), reinterpret_cast<void*>(resolved), 0, &resolved_mbi, sizeof(resolved_mbi), &resolved_length) < 0
				|| resolved_mbi.State != MEM_COMMIT
				|| !(resolved_mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
			{
				return false;
			}

			const auto region_end = reinterpret_cast<std::uintptr_t>(resolved_mbi.BaseAddress) + resolved_mbi.RegionSize;
			if (resolved > region_end || region_end - resolved < 6)
			{
				return false;
			}

			const auto byte = *reinterpret_cast<std::uint8_t*>(resolved);

			if (byte == 0xe9)
			{
				const auto rel = *reinterpret_cast<std::int32_t*>(resolved + 1);
				resolved = resolved + 5 + rel;
			}
			else if (byte == 0xff && *reinterpret_cast<std::uint8_t*>(resolved + 1) == 0x25)
			{
				const auto ptr = resolved + 6 + *reinterpret_cast<std::int32_t*>(resolved + 2);
				resolved = *reinterpret_cast<std::uintptr_t*>(ptr);
			}
			else
			{
				break;
			}

			if (depth == 7)
			{
				return false;
			}
		}

		target = reinterpret_cast<void*>(resolved);

		MEMORY_BASIC_INFORMATION mbi{};
		auto return_length{ 0ull };

		if (detail::nt_query_virtual_memory(GetCurrentProcess(), target, 0, &mbi, sizeof(mbi), &return_length) < 0)
		{
			return false;
		}

		if (mbi.State != MEM_COMMIT)
		{
			return false;
		}

		if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
		{
			return false;
		}

		const auto trampoline = allocator::allocate(64, target);
		if (!trampoline)
		{
			return false;
		}

		std::memset(trampoline, 0xCC, 64);

		auto original_len{ 0ull };
		auto trampoline_len{ 0ull };

		if (!detail::build_trampoline(target, trampoline, &original_len, &trampoline_len))
		{
			allocator::free(trampoline);
			return false;
		}

		detail::flush_icode(trampoline, trampoline_len);

		const auto patch_size = detail::write_jmp_auto(this->m_hook_bytes, static_cast<std::uint8_t*>(target) + 5, hook_function);
		std::memcpy(this->m_original_bytes, target, original_len);

		this->m_target = target;
		this->m_hook = hook_function;
		this->m_trampoline = trampoline;
		this->m_original_length = original_len;
		this->m_patch_size = patch_size;
		this->m_enabled = false;

		return true;
	}

	bool jmp::enable()
	{
		if (!this->is_valid())
		{
			return false;
		}

		if (this->m_enabled)
		{
			return true;
		}

		auto base = this->m_target;
		auto size = this->m_patch_size;
		auto old_protect{ 0ul };

		if (detail::nt_protect_virtual_memory(GetCurrentProcess(), &base, &size, PAGE_EXECUTE_READWRITE, &old_protect) < 0)
		{
			return false;
		}

		std::memcpy(this->m_target, this->m_hook_bytes, this->m_patch_size);

		detail::flush_icode(this->m_target, this->m_patch_size);

		detail::nt_protect_virtual_memory(GetCurrentProcess(), &base, &size, old_protect, &old_protect);

		this->m_enabled = true;
		return true;
	}

	bool jmp::disable()
	{
		if (!this->is_valid())
		{
			return false;
		}

		if (!this->m_enabled)
		{
			return true;
		}

		auto base = this->m_target;
		auto size = this->m_original_length;
		auto old_protect{ 0ul };

		if (detail::nt_protect_virtual_memory(GetCurrentProcess(), &base, &size, PAGE_EXECUTE_READWRITE, &old_protect) < 0)
		{
			return false;
		}

		std::memcpy(this->m_target, this->m_original_bytes, this->m_original_length);

		detail::flush_icode(this->m_target, this->m_original_length);

		detail::nt_protect_virtual_memory(GetCurrentProcess(), &base, &size, old_protect, &old_protect);

		this->m_enabled = false;
		return true;
	}

	void jmp::reset()
	{
		if (!this->is_valid())
		{
			return;
		}

		this->disable();

		if (this->m_trampoline)
		{
			allocator::free(this->m_trampoline);
		}

		this->m_target = nullptr;
		this->m_hook = nullptr;
		this->m_trampoline = nullptr;
		this->m_original_length = 0;
		this->m_patch_size = 0;
		this->m_enabled = false;

		std::memset(this->m_original_bytes, 0, sizeof(this->m_original_bytes));
		std::memset(this->m_hook_bytes, 0, sizeof(this->m_hook_bytes));
	}

}

namespace hooking::manager {

	bool create(const std::initializer_list<entry>& entries)
	{
		for (const auto& entry : entries)
		{
			if (!entry.hook || !entry.detour)
			{
				return false;
			}

			if (!entry.address)
			{
				return false;
			}
		}

		const auto rollback = [&entries]()
			{
				for (const auto& entry : entries)
				{
					if (entry.hook)
					{
						entry.hook->reset();
					}
				}
			};

		for (const auto& entry : entries)
		{
			if (!entry.hook->create(reinterpret_cast<void*>(entry.address), entry.detour))
			{
				rollback();
				return false;
			}

			if (!entry.hook->enable())
			{
				rollback();
				return false;
			}

			security::prologues::add(entry.address, entry.hook->get_original_bytes(), entry.hook->get_original_length());
		}

		return true;
	}

}

namespace {

	void append_u16_le(std::vector<unsigned char>& out, std::uint16_t value)
	{
		out.push_back(static_cast<unsigned char>(value & 0xFF));
		out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
	}

	void append_u32_le(std::vector<unsigned char>& out, std::uint32_t value)
	{
		out.push_back(static_cast<unsigned char>(value & 0xFF));
		out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
		out.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
		out.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
	}

	void append_f32_zero(std::vector<unsigned char>& out)
	{
		append_u32_le(out, 0u);
	}

	struct embedded_panorama_logo_texture
	{
		std::uint16_t width{};
		std::uint16_t height{};
		std::vector<unsigned char> rgba{};
		std::vector<std::vector<unsigned char>> mipmaps{};
		std::vector<unsigned char> vtex{};
		bool valid{};
	};

	std::string normalize_resource_path(std::string_view path)
	{
		std::string normalized{};
		normalized.reserve(path.size());

		for (const auto ch : path)
		{
			if (ch == '\\')
			{
				normalized.push_back('/');
			}
			else
			{
				normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
			}
		}

		return normalized;
	}

	std::vector<std::vector<unsigned char>> generate_rgba_mipmaps(
		const std::vector<unsigned char>& base_level,
		std::uint16_t width,
		std::uint16_t height)
	{
		std::vector<std::vector<unsigned char>> mipmaps{};
		if (base_level.empty() || width == 0u || height == 0u)
		{
			return mipmaps;
		}

		mipmaps.push_back(base_level);

		auto current_width = static_cast<std::size_t>(width);
		auto current_height = static_cast<std::size_t>(height);

		while (current_width > 1u || current_height > 1u)
		{
			const auto next_width = std::max<std::size_t>(1u, current_width / 2u);
			const auto next_height = std::max<std::size_t>(1u, current_height / 2u);

			const auto& src = mipmaps.back();
			std::vector<unsigned char> dst(next_width * next_height * 4u);

			for (std::size_t y = 0; y < next_height; ++y)
			{
				for (std::size_t x = 0; x < next_width; ++x)
				{
					std::uint32_t accum[4]{};
					std::uint32_t samples{};

					for (std::size_t sample_y = 0; sample_y < 2u; ++sample_y)
					{
						const auto src_y = std::min(current_height - 1u, y * 2u + sample_y);
						for (std::size_t sample_x = 0; sample_x < 2u; ++sample_x)
						{
							const auto src_x = std::min(current_width - 1u, x * 2u + sample_x);
							const auto src_index = (src_y * current_width + src_x) * 4u;

							for (auto channel = 0; channel < 4; ++channel)
							{
								accum[channel] += src[src_index + static_cast<std::size_t>(channel)];
							}

							++samples;
						}
					}

					const auto dst_index = (y * next_width + x) * 4u;
					for (auto channel = 0; channel < 4; ++channel)
					{
						dst[dst_index + static_cast<std::size_t>(channel)] =
							static_cast<unsigned char>(accum[channel] / samples);
					}
				}
			}

			mipmaps.push_back(std::move(dst));
			current_width = next_width;
			current_height = next_height;
		}

		return mipmaps;
	}

	embedded_panorama_logo_texture build_embedded_panorama_logo_texture()
	{
		const auto png = std::span<const unsigned char>{
			features::misc::scoreboard_logo::k_png_bytes,
			features::misc::scoreboard_logo::k_png_size
		};
		embedded_panorama_logo_texture texture{};

		if (png.size() < 24)
		{
			return texture;
		}

		const auto is_png =
			png[0] == 0x89u &&
			png[1] == 0x50u &&
			png[2] == 0x4Eu &&
			png[3] == 0x47u &&
			png[4] == 0x0Du &&
			png[5] == 0x0Au &&
			png[6] == 0x1Au &&
			png[7] == 0x0Au;

		if (!is_png)
		{
			return texture;
		}

		auto uninitialize_com = false;
		const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (SUCCEEDED(hr))
		{
			uninitialize_com = true;
		}
		else if (hr != RPC_E_CHANGED_MODE)
		{
			return texture;
		}

		IWICImagingFactory* factory{};
		IWICStream* stream{};
		IWICBitmapDecoder* decoder{};
		IWICBitmapFrameDecode* frame{};
		IWICFormatConverter* converter{};

		const auto release_all = [&]()
			{
				if (converter)
					converter->Release();
				if (frame)
					frame->Release();
				if (decoder)
					decoder->Release();
				if (stream)
					stream->Release();
				if (factory)
					factory->Release();
				if (uninitialize_com)
					CoUninitialize();
			};

		if (FAILED(CoCreateInstance(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&factory))))
		{
			release_all();
			return texture;
		}

		if (FAILED(factory->CreateStream(&stream)) ||
			FAILED(stream->InitializeFromMemory(
				const_cast<WICInProcPointer>(reinterpret_cast<const BYTE*>(png.data())),
				static_cast<DWORD>(png.size()))) ||
			FAILED(factory->CreateDecoderFromStream(
				stream,
				nullptr,
				WICDecodeMetadataCacheOnLoad,
				&decoder)) ||
			FAILED(decoder->GetFrame(0, &frame)))
		{
			release_all();
			return texture;
		}

		UINT width{};
		UINT height{};
		if (FAILED(frame->GetSize(&width, &height)) ||
			width == 0u || height == 0u ||
			width > std::numeric_limits<std::uint16_t>::max() ||
			height > std::numeric_limits<std::uint16_t>::max())
		{
			release_all();
			return texture;
		}

		if (FAILED(factory->CreateFormatConverter(&converter)) ||
			FAILED(converter->Initialize(
				frame,
				GUID_WICPixelFormat32bppRGBA,
				WICBitmapDitherTypeNone,
				nullptr,
				0.0,
				WICBitmapPaletteTypeCustom)))
		{
			release_all();
			return texture;
		}

		const auto stride = static_cast<std::size_t>(width) * 4u;
		texture.rgba.resize(stride * static_cast<std::size_t>(height));
		if (FAILED(converter->CopyPixels(
			nullptr,
			static_cast<UINT>(stride),
			static_cast<UINT>(texture.rgba.size()),
			reinterpret_cast<BYTE*>(texture.rgba.data()))))
		{
			texture.rgba.clear();
			release_all();
			return texture;
		}

		release_all();

		texture.width = static_cast<std::uint16_t>(width);
		texture.height = static_cast<std::uint16_t>(height);
		texture.mipmaps = generate_rgba_mipmaps(texture.rgba, texture.width, texture.height);
		texture.valid = true;

		constexpr std::uint32_t k_data_block_type =
			static_cast<std::uint32_t>('D') |
			(static_cast<std::uint32_t>('A') << 8) |
			(static_cast<std::uint32_t>('T') << 16) |
			(static_cast<std::uint32_t>('A') << 24);
		constexpr std::uint16_t k_header_version = 12u;
		constexpr std::uint16_t k_texture_version = 1u;
		constexpr std::uint32_t k_block_offset = 8u;
		constexpr std::uint32_t k_block_count = 1u;
		constexpr std::uint32_t k_data_block_start = 32u;
		constexpr std::uint32_t k_data_block_size = 40u;
		constexpr std::uint32_t k_pixel_data_offset = k_data_block_start + k_data_block_size;
		constexpr std::uint8_t k_format_rgba8888 = 4u;
		const auto mip_levels = static_cast<std::uint8_t>(std::min<std::size_t>(255u, texture.mipmaps.size()));

		std::size_t pixel_data_size{};
		for (const auto& mip : texture.mipmaps)
		{
			pixel_data_size += mip.size();
		}

		texture.vtex.reserve(k_pixel_data_offset + pixel_data_size);
		append_u32_le(texture.vtex, k_pixel_data_offset);
		append_u16_le(texture.vtex, k_header_version);
		append_u16_le(texture.vtex, k_texture_version);
		append_u32_le(texture.vtex, k_block_offset);
		append_u32_le(texture.vtex, k_block_count);

		append_u32_le(texture.vtex, k_data_block_type);
		append_u32_le(texture.vtex, k_data_block_start - 20u);
		append_u32_le(texture.vtex, k_data_block_size);

		while (texture.vtex.size() < k_data_block_start)
		{
			texture.vtex.push_back(0u);
		}

		append_u16_le(texture.vtex, k_texture_version);
		append_u16_le(texture.vtex, 0u);

		for (auto i = 0; i < 4; ++i)
		{
			append_f32_zero(texture.vtex);
		}

		append_u16_le(texture.vtex, texture.width);
		append_u16_le(texture.vtex, texture.height);
		append_u16_le(texture.vtex, 1u);
		texture.vtex.push_back(k_format_rgba8888);
		texture.vtex.push_back(mip_levels);
		append_u32_le(texture.vtex, static_cast<std::uint32_t>(texture.width) * static_cast<std::uint32_t>(texture.height));
		append_u32_le(texture.vtex, 0u);
		append_u32_le(texture.vtex, 0u);

		for (auto it = texture.mipmaps.rbegin(); it != texture.mipmaps.rend(); ++it)
		{
			texture.vtex.insert(texture.vtex.end(), it->begin(), it->end());
		}

		return texture;
	}

	std::span<const unsigned char> find_embedded_particle(const std::string& filename)
	{
		auto name = filename;
		const auto slash = name.find_last_of("/\\");
		if (slash != std::string::npos)
			name = name.substr(slash + 1);

		const auto dot = name.find('.');
		if (dot != std::string::npos)
			name = name.substr(0, dot);

		using span_t = std::span<const unsigned char>;
		struct entry { const char* key; span_t data; };
		static const entry table[] = {
			{ "rain", span_t{ resources::particles::weather::rain } },
			{ "snow", span_t{ resources::particles::weather::snow } },
			{ "stars", span_t{ resources::particles::weather::stars } },
			{ "killstars", span_t{ resources::particles::effects::killstars } },
			{ "tracer", span_t{ resources::particles::effects::tracer } },
			{ "sparks", span_t{ resources::particles::effects::sparks } },
			{ "fade", span_t{ resources::particles::effects::fade } },
			{ "halo", span_t{ resources::particles::effects::halo } },
			{ "autopeek", span_t{ resources::particles::custom::autopeek } },
			{ "bluetime", span_t{ resources::particles::custom::bluetime } },
			{ "greentime", span_t{ resources::particles::custom::greentime } },
			{ "purpletime", span_t{ resources::particles::custom::purpletime } },
			{ "falling_ember1", span_t{ resources::particles::custom::falling_ember1 } },
			{ "falling_ember2", span_t{ resources::particles::custom::falling_ember2 } },
			{ "falling_snow1", span_t{ resources::particles::custom::falling_snow1 } },
			{ "snowfall", span_t{ resources::particles::custom::snow } },
			{ "ss_rain", span_t{ resources::particles::custom::ss_rain } },
			{ "nomove_stars", span_t{ resources::particles::custom::nomove_stars } },
			{ "lightning_kill_blue", span_t{ resources::particles::custom::lightning_kill_blue } },
			{ "lightning_kill_green", span_t{ resources::particles::custom::lightning_kill_green } },
			{ "lightning_kill_purple", span_t{ resources::particles::custom::lightning_kill_purple } },
			{ "explosionblue", span_t{ resources::particles::custom::explosionblue } },
			{ "explosiongreen", span_t{ resources::particles::custom::explosiongreen } },
			{ "explosionpurple", span_t{ resources::particles::custom::explosionpurple } },
			{ "explosion_particle_with_color", span_t{ resources::particles::custom::explosion_particle_with_color } },
			{ "poxian_exp", span_t{ resources::particles::custom::poxian_exp } },
			{ "spectator_utility_trail", span_t{ resources::particles::custom::spectator_utility_trail } },
		};

		for (const auto& e : table)
		{
			if (name == e.key)
				return e.data;
		}

		return {};
	}

	std::span<const unsigned char> find_embedded_panorama_image(const std::string& filename)
	{
		static constexpr std::string_view k_logo_png_suffix{ "/embedded/cs2_internal_sb_logo.png" };
		static constexpr std::string_view k_logo_vtex_suffix{ "/embedded/cs2_internal_sb_logo_png.vtex_c" };

		const auto normalized = normalize_resource_path(filename);
		const auto normalized_view = std::string_view{ normalized };

		if (normalized_view.ends_with(k_logo_vtex_suffix))
		{
			static const auto texture = build_embedded_panorama_logo_texture();
			if (texture.valid)
				return texture.vtex;

			return {};
		}

		if (normalized_view.ends_with(k_logo_png_suffix))
		{
			return std::span<const unsigned char>{
				features::misc::scoreboard_logo::k_png_bytes,
					features::misc::scoreboard_logo::k_png_size
			};
		}

		return {};
	}

}

namespace hooks {

	bool utility::initialize()
	{
		if (!hooking::manager::create({
			{ &m_service_read, &service_read, xs("service_read"), PATTERN(PATTERN_SERVICE_READ) },
			{ &m_log_internal, &log_internal, xs("log_internal"), PATTERN(PATTERN_LOG_INTERNAL) }
			}))
		{
			return false;
		}

		return true;
	}

	void utility::shutdown()
	{
		m_service_read.reset();
		m_log_internal.reset();
	}

	std::uintptr_t __fastcall utility::service_read(std::uintptr_t a1)
	{
		const auto flags_len = memory::read<std::uint32_t>(a1 - 212);
		const auto len = flags_len & 0x3fffffff;

		std::string filename;
		if (len > 0 && len < 512)
		{
			char buffer[512]{};

			if (flags_len & 0x40000000)
			{
				std::memcpy(buffer, reinterpret_cast<void*>(a1 - 208), std::min(len, 511u));
			}
			else
			{
				const auto string = memory::read<std::uintptr_t>(a1 - 208);
				if (string)
				{
					std::memcpy(buffer, reinterpret_cast<void*>(string), std::min(len, 511u));
				}
			}

			filename = buffer;
		}

		const auto normalized_filename = normalize_resource_path(filename);

		if (normalized_filename.find("panorama/images/embedded/") != std::string::npos)
		{
			const auto image = find_embedded_panorama_image(filename);
			const auto async_filesystem = memory::read<std::uintptr_t>(a1 + 24);

			if (!image.empty() && async_filesystem)
			{
				const auto buffer = memory::call_vfunc<std::uintptr_t>(async_filesystem, 22, image.size(), filename.c_str());
				if (buffer)
				{
					std::memcpy(reinterpret_cast<void*>(buffer), image.data(), image.size());

					memory::write<std::uintptr_t>(a1 + 56, buffer);
					memory::write<std::uintptr_t>(a1 + 64, image.size());
					memory::write<std::uintptr_t>(a1 + 72, image.size());
					memory::call<void>(PATTERN(PATTERN_FILESYSTEM_CLOSE), a1 - 224, 0);

					return 0;
				}
			}
		}

		if (normalized_filename.find("particles/embedded/") != std::string::npos)
		{
			const auto particle = find_embedded_particle(filename);
			const auto async_filesystem = memory::read<std::uintptr_t>(a1 + 24);

			if (!particle.empty() && async_filesystem)
			{
				const auto buffer = memory::call_vfunc<std::uintptr_t>(async_filesystem, 22, particle.size(), filename.c_str());
				if (buffer)
				{
					std::memcpy(reinterpret_cast<void*>(buffer), particle.data(), particle.size());

					memory::write<std::uintptr_t>(a1 + 56, buffer);
					memory::write<std::uintptr_t>(a1 + 64, particle.size());
					memory::write<std::uintptr_t>(a1 + 72, particle.size());
					memory::call<void>(PATTERN(PATTERN_FILESYSTEM_CLOSE), a1 - 224, 0);

					return 0;
				}
			}
		}

		return m_service_read.call<std::uintptr_t>(a1);
	}

	std::intptr_t __fastcall utility::log_internal(std::uintptr_t a1, std::uint32_t channel, std::int32_t severity, std::uintptr_t metadata, const char* message, std::intptr_t* args)
	{
		if (settings::g_misc.disable_game_logs)
		{
			return 0;
		}

		return m_log_internal.call<std::intptr_t>(a1, channel, severity, metadata, message, args);
	}

}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace hooks {

	namespace {

		[[nodiscard]] bool world_busy( )
		{
			return systems::g_lifecycle.busy( );
		}

		[[nodiscard]] bool scene_mod_busy( )
		{
			return world_busy( ) || features::world::g_scene.settling( );
		}

		[[nodiscard]] bool chams_generate_needed( )
		{
			const auto& chams = settings::g_esp.m_player.m_chams;
			const auto& viewmodel = settings::g_esp.m_viewmodel;
			if ( chams.enemy.enabled.value || chams.team.enabled.value || chams.local.enabled.value
				|| chams.enemy_ragdoll.enabled.value || chams.team_ragdoll.enabled.value || chams.local_ragdoll.enabled.value
				|| chams.backtrack.enabled.value || chams.onshot.enabled.value
				|| viewmodel.weapon.enabled.value || viewmodel.arms.enabled.value || viewmodel.gloves.enabled.value )
			{
				return true;
			}

			return rendering::g_menu.is_open( );
		}

		[[nodiscard]] bool game_input_blocked( )
		{
			if ( rendering::g_menu.is_open( ) )
			{
				return true;
			}

			const auto hwnd = rendering::g_context.get_window( );
			if ( !hwnd || !IsWindow( hwnd ) )
			{
				return true;
			}

			return GetForegroundWindow( ) != hwnd;
		}

		void drop_world_runtime( bool leaving_match )
		{
			features::changer::custom_paint::on_level_end( );
			features::changer::g_guns.on_level_end( );
			features::changer::g_knives.on_level_end( );
			features::changer::g_gloves.on_level_end( );

			if ( leaving_match && economy::g_tracker.is_match_active( ) )
			{
				economy::g_tracker.on_match_end( );
			}

			rendering::g_widgets.s_map_name.clear( );

			features::world::g_scene.invalidate_runtime( true );
			features::world::g_weather.release( );
			features::misc::g_impacts.on_level_change( );
			features::misc::g_onshot.on_level_change( );
			features::misc::g_scoreboard_weapons.on_level_change( );
			features::misc::g_dlight.on_level_shutdown( );
			particles::clear( );
			particles::reset( );
			features::world::g_ground_particles.reset( );
			features::world::g_hitkill_particles.reset( );
			features::world::g_throwable_trail.forget( );
			features::esp::player::g_chams.bt( ).shutdown( true );
			features::esp::player::g_chams.os( ).shutdown( true );
			features::misc::g_camera.reset_freecam( );
			features::visuals::model_preview::on_level_transition( );

			systems::g_entities.clear( );
			features::combat::g_shared.lc( ).clear( );
			systems::g_local.reset( );
			systems::g_view.invalidate( );
			systems::g_frame_data.reset( );
			features::combat::g_shared.invalidate_if_needed( );
			features::combat::g_legit.invalidate_if_needed( );
			features::combat::g_misc.autopeak( ).reset_if_needed( );
		}

	}

	bool cheat::initialize() {
		if (!hooking::manager::create({
			{ &m_present, &present, xs("present"), addresses::functions::present },
			{ &m_resize_buffers, &resize_buffers, xs("resize_buffers"), addresses::functions::resize_buffers }
			})) {
			return false;
		}

		const hooking::manager::entry feature_hooks[]{
			{ &m_cmd_interpreter, &cmd_interpreter, xs("cmd_interpreter"), PATTERN(PATTERN_CMD_INTERPRETER) },
			{ &m_frame_stage_notify, &frame_stage_notify, xs("frame_stage_notify"), PATTERN(PATTERN_FRAME_STAGE_NOTIFY) },
			{ &m_create_move, &create_move, xs("create_move"), PATTERN(PATTERN_CREATE_MOVE) },
			{ &m_handle_view_angles, &handle_view_angles, xs("handle_view_angles"), PATTERN(PATTERN_HANDLE_VIEW_ANGLES) },
			{ &m_add_entity, &add_entity, xs("add_entity"), PATTERN(PATTERN_ADD_ENTITY) },
			{ &m_remove_entity, &remove_entity, xs("remove_entity"), PATTERN(PATTERN_REMOVE_ENTITY) },
			{ &m_render_view, &render_view, xs("render_view"), PATTERN(PATTERN_RENDER_VIEW) },
			{ &m_draw_skybox_array, &draw_skybox_array, xs("draw_skybox_array"), PATTERN(PATTERN_DRAW_SKYBOX_ARRAY) },
			{ &m_aggregate_draw_array, &aggregate_draw_array, xs("aggregate_draw_array"), PATTERN(PATTERN_AGGREGATE_DRAW_ARRAY) },
			{ &m_light_scene_object, &light_scene_object, xs("light_scene_object"), PATTERN(PATTERN_LIGHT_SCENE_OBJECT) },
			{ &m_draw_scene_object_array, &draw_scene_object_array, xs("draw_scene_object_array"), PATTERN(PATTERN_DRAW_SCENE_OBJECT_ARRAY) },
			{ &m_draw_scene_object, &draw_scene_object, xs("draw_scene_object"), PATTERN(PATTERN_DRAW_SCENE_OBJECT) },
			{ &m_is_glowing, &is_glowing, xs("is_glowing"), PATTERN(PATTERN_IS_GLOWING) },
			{ &m_get_glow_color, &get_glow_color, xs("get_glow_color"), PATTERN(PATTERN_GET_GLOW_COLOR) },
			{ &m_generate_primitives, &generate_primitives, xs("generate_primitives"), PATTERN(PATTERN_GENERATE_PRIMITIVES) },
			{ &m_get_weapon_type, &get_weapon_type, xs("get_weapon_type"), PATTERN(PATTERN_GET_WEAPON_TYPE) },
			{ &m_get_resource_view, &get_resource_view, xs("get_resource_view"), PATTERN(PATTERN_GET_RESOURCE_VIEW) },
			{ &m_parse_report_hit, &parse_report_hit, xs("parse_report_hit"), PATTERN(PATTERN_PARSE_REPORT_HIT) },
			{ &m_setup_fog, &setup_fog, xs("setup_fog"), PATTERN(PATTERN_SETUP_FOG) },
			{ &m_set_shader_param, &set_shader_param, xs("set_shader_param"), PATTERN(PATTERN_SET_SHADER_PARAM) },
			{ &m_set_postprocess_vec, &set_postprocess_vec, xs("set_postprocess_vec"), PATTERN(PATTERN_SET_POSTPROCESS_VEC) },
			{ &m_override_view, &override_view, xs("override_view"), PATTERN(PATTERN_OVERRIDE_VIEW) },
			{ &m_update_fov_sensitivity, &update_fov_sensitivity, xs("update_fov_sensitivity"), PATTERN(PATTERN_UPDATE_FOV_SENSITIVITY) },
			{ &m_render_scope, &render_scope, xs("render_scope"), PATTERN(PATTERN_RENDER_SCOPE) },
			{ &m_render_crosshair, &render_crosshair, xs("render_crosshair"), PATTERN(PATTERN_RENDER_CROSSHAIR) },
			{ &m_weapon_hides_crosshair, &weapon_hides_crosshair, xs("weapon_hides_crosshair"), PATTERN(PATTERN_WEAPON_HIDES_CROSSHAIR) },
			{ &m_prepare_scene_material, &prepare_scene_material, xs("prepare_scene_material"), PATTERN(PATTERN_PREPARE_SCENE_MATERIAL) },
			{ &m_post_network_data_received, &post_network_data_received, xs("post_network_data_received"), PATTERN(PATTERN_POST_NETWORK_DATA_RECEIVED) },
			{ &m_draw_overhead, &draw_overhead, xs("draw_overhead"), PATTERN(PATTERN_DRAW_OVERHEAD) },
			{ &m_draw_legs, &draw_legs, xs("draw_legs"), PATTERN(PATTERN_DRAW_LEGS) },
			{ &m_get_transforms_for_hitbox_list, &get_transforms_for_hitbox_list, xs("get_transforms_for_hitbox_list"), PATTERN(PATTERN_GET_TRANSFORMS_FOR_HITBOX_LIST) },
			{ &m_sort_primitives, &sort_primitives, xs("sort_primitives"), PATTERN(PATTERN_SORT_PRIMITIVES) },
			{ &m_get_interpolated_shoot_position, &get_interpolated_shoot_position, xs("get_interpolated_shoot_position"), PATTERN(PATTERN_GET_INTERPOLATED_SHOOT_POSITION) },
			{ &m_level_initialization, &level_initialization, xs("level_initialization"), PATTERN(PATTERN_LEVEL_INITIALIZATION) },
			{ &m_level_shutdown, &level_shutdown, xs("level_shutdown"), PATTERN(PATTERN_LEVEL_SHUTDOWN) },
			{ &m_read_frame_input, &read_frame_input, xs("read_frame_input"), PATTERN(PATTERN_READ_FRAME_INPUT) },
			{ &m_process_input_event, &process_input_event, xs("process_input_event"), PATTERN(PATTERN_PROCESS_INPUT_EVENT) },
			{ &m_render_decals, &render_decals, xs("render_decals"), PATTERN(PATTERN_RENDER_DECALS) },
			{ &m_render_smoke, &render_smoke, xs("render_smoke"), PATTERN(PATTERN_RENDER_SMOKE) },
			{ &m_draw_flash_effect, &draw_flash_effect, xs("draw_flash_effect"), PATTERN(PATTERN_DRAW_FLASH_EFFECT) },
			{ &m_set_info, &set_info, xs("set_info"), PATTERN(PATTERN_SET_INFO) },
			{ &m_viewmodel_update_and_setup_view, &viewmodel_update_and_setup_view, xs("viewmodel_update_and_setup_view"), PATTERN(PATTERN_VIEWMODEL_UPDATE_AND_SETUP_VIEW) },
			{ &m_particle_draw_array, &particle_draw_array, xs("particle_draw_array"), PATTERN(PATTERN_PARTICLE_DRAW_ARRAY) }
		};

		for (const auto& entry : feature_hooks) {
			hooking::manager::create({ entry });
		}

		{
			const auto branch_addr = PATTERN(PATTERN_FORCE_VIS_MATERIAL_BRANCH);

			if (branch_addr)
			{
				m_force_vis_label76 = branch_addr + 7 + 2 + 0x2F;
				m_force_vis_material_branch_stub = hooking::allocator::allocate(
					96, reinterpret_cast<void*>(branch_addr));
				if (m_force_vis_material_branch_stub
					&& m_force_vis_material_branch.create(
						reinterpret_cast<void*>(branch_addr), m_force_vis_material_branch_stub))
				{
					auto* stub = static_cast<std::uint8_t*>(m_force_vis_material_branch_stub);
					std::size_t o{};

					stub[o++] = 0x50;
					stub[o++] = 0x51;
					stub[o++] = 0x52;
					stub[o++] = 0x41; stub[o++] = 0x50;
					stub[o++] = 0x41; stub[o++] = 0x51;
					stub[o++] = 0x48; stub[o++] = 0x83; stub[o++] = 0xEC; stub[o++] = 0x28;
					stub[o++] = 0x48; stub[o++] = 0xB8;
					*reinterpret_cast<std::uintptr_t*>(stub + o) =
						reinterpret_cast<std::uintptr_t>(&world_material_swap_active);
					o += 8;
					stub[o++] = 0xFF; stub[o++] = 0xD0;
					stub[o++] = 0x48; stub[o++] = 0x83; stub[o++] = 0xC4; stub[o++] = 0x28;
					stub[o++] = 0x41; stub[o++] = 0x59;
					stub[o++] = 0x41; stub[o++] = 0x58;
					stub[o++] = 0x5A;
					stub[o++] = 0x59;
					stub[o++] = 0x84; stub[o++] = 0xC0;
					stub[o++] = 0x58;
					stub[o++] = 0x74; stub[o++] = 0x0E;
					stub[o++] = 0xFF; stub[o++] = 0x25;
					*reinterpret_cast<std::int32_t*>(stub + o) = 0;
					o += 4;
					*reinterpret_cast<std::uintptr_t*>(stub + o) = m_force_vis_label76;
					o += 8;
					stub[o++] = 0xFF; stub[o++] = 0x25;
					*reinterpret_cast<std::int32_t*>(stub + o) = 0;
					o += 4;
					*reinterpret_cast<std::uintptr_t*>(stub + o) =
						reinterpret_cast<std::uintptr_t>(m_force_vis_material_branch.get_trampoline());
					o += 8;

					hooking::detail::flush_icode(stub, o);

					if (!m_force_vis_material_branch.enable())
					{
						m_force_vis_material_branch.reset();
						hooking::allocator::free(m_force_vis_material_branch_stub);
						m_force_vis_material_branch_stub = nullptr;
						m_force_vis_label76 = 0;
					}
					else
					{
						security::prologues::add(
							branch_addr,
							m_force_vis_material_branch.get_original_bytes(),
							m_force_vis_material_branch.get_original_length());
					}
				}
				else
				{
					if (m_force_vis_material_branch_stub)
					{
						hooking::allocator::free(m_force_vis_material_branch_stub);
						m_force_vis_material_branch_stub = nullptr;
					}
					m_force_vis_material_branch.reset();
					m_force_vis_label76 = 0;
				}
			}
		}

		{
			const auto force_addr = PATTERN(PATTERN_FORCE_WORLD_MATERIAL);

			if (force_addr)
			{
				m_force_world_material_stub = hooking::allocator::allocate(128, reinterpret_cast<void*>(force_addr));
				if (m_force_world_material_stub
					&& m_force_world_material.create(
						reinterpret_cast<void*>(force_addr), m_force_world_material_stub))
				{
					auto* stub = static_cast<std::uint8_t*>(m_force_world_material_stub);
					std::size_t o{};

					stub[o++] = 0x53;
					stub[o++] = 0x51;
					stub[o++] = 0x52;
					stub[o++] = 0x41; stub[o++] = 0x50;
					stub[o++] = 0x41; stub[o++] = 0x51;
					stub[o++] = 0x41; stub[o++] = 0x52;
					stub[o++] = 0x41; stub[o++] = 0x53;
					stub[o++] = 0x48; stub[o++] = 0x83; stub[o++] = 0xEC; stub[o++] = 0x28;
					stub[o++] = 0x48; stub[o++] = 0x89; stub[o++] = 0xD9;
					stub[o++] = 0x48; stub[o++] = 0xB8;
					*reinterpret_cast<std::uintptr_t*>(stub + o) =
						reinterpret_cast<std::uintptr_t>(&force_world_material_rcx);
					o += 8;
					stub[o++] = 0xFF; stub[o++] = 0xD0;
					stub[o++] = 0x48; stub[o++] = 0x83; stub[o++] = 0xC4; stub[o++] = 0x28;
					stub[o++] = 0x41; stub[o++] = 0x5B;
					stub[o++] = 0x41; stub[o++] = 0x5A;
					stub[o++] = 0x41; stub[o++] = 0x59;
					stub[o++] = 0x41; stub[o++] = 0x58;
					stub[o++] = 0x5A;
					stub[o++] = 0x59;
					stub[o++] = 0x5B;
					stub[o++] = 0x48; stub[o++] = 0x85; stub[o++] = 0xC0;
					stub[o++] = 0x74; stub[o++] = 0x03;
					stub[o++] = 0x48; stub[o++] = 0x89; stub[o++] = 0xC1;
					stub[o++] = 0xFF; stub[o++] = 0x25;
					*reinterpret_cast<std::int32_t*>(stub + o) = 0;
					o += 4;
					*reinterpret_cast<std::uintptr_t*>(stub + o) =
						reinterpret_cast<std::uintptr_t>(m_force_world_material.get_trampoline());
					o += 8;

					hooking::detail::flush_icode(stub, o);

					if (!m_force_world_material.enable())
					{
						m_force_world_material.reset();
						hooking::allocator::free(m_force_world_material_stub);
						m_force_world_material_stub = nullptr;
					}
					else
					{
						security::prologues::add(
							force_addr,
							m_force_world_material.get_original_bytes(),
							m_force_world_material.get_original_length());
					}
				}
				else
				{
					if (m_force_world_material_stub)
					{
						hooking::allocator::free(m_force_world_material_stub);
						m_force_world_material_stub = nullptr;
					}
					m_force_world_material.reset();
				}
			}
		}

		particles::setup();
		return true;
	}

	void cheat::shutdown()
	{
		systems::g_lifecycle.begin();
		drop_world_runtime(true);
		if (m_wnd_proc_hwnd && m_wnd_proc_orig && IsWindow(m_wnd_proc_hwnd))
			SetWindowLongPtrW(m_wnd_proc_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_wnd_proc_orig));
		m_wnd_proc_hwnd = nullptr;
		m_wnd_proc_orig = nullptr;
		m_present.reset();
		m_resize_buffers.reset();
		m_cmd_interpreter.reset();
		m_frame_stage_notify.reset();
		m_create_move.reset();
		m_handle_view_angles.reset();
		m_add_entity.reset();
		m_remove_entity.reset();
		m_render_view.reset();
		m_draw_skybox_array.reset();
		m_aggregate_draw_array.reset();
		m_light_scene_object.reset();
		m_draw_scene_object_array.reset();
		m_draw_scene_object.reset();
		m_force_vis_material_branch.reset();
		if (m_force_vis_material_branch_stub)
		{
			hooking::allocator::free(m_force_vis_material_branch_stub);
			m_force_vis_material_branch_stub = nullptr;
		}
		m_force_vis_label76 = 0;
		m_force_world_material.reset();
		if (m_force_world_material_stub)
		{
			hooking::allocator::free(m_force_world_material_stub);
			m_force_world_material_stub = nullptr;
		}
		m_is_glowing.reset();
		m_get_glow_color.reset();
		m_generate_primitives.reset();
		m_get_weapon_type.reset();
		m_get_resource_view.reset();
		m_parse_report_hit.reset();
		m_setup_fog.reset();
		m_set_shader_param.reset();
		m_set_postprocess_vec.reset();
		m_override_view.reset();
		m_update_fov_sensitivity.reset();
		m_render_scope.reset();
		m_render_crosshair.reset();
		m_weapon_hides_crosshair.reset();
		m_prepare_scene_material.reset();
		m_post_network_data_received.reset();
		m_draw_overhead.reset();
		m_draw_legs.reset();
		m_get_transforms_for_hitbox_list.reset();
		m_sort_primitives.reset();
		m_get_inaccuracy.reset();
		m_get_interpolated_shoot_position.reset();
		m_level_initialization.reset();
		m_level_shutdown.reset();
		m_read_frame_input.reset();
		m_process_input_event.reset();
		m_render_decals.reset();
		m_render_smoke.reset();
		m_render_smoke_map.reset();
		m_render_smoke_unmap.reset();
		m_draw_flash_effect.reset();
		m_set_info.reset();
		m_viewmodel_update_and_setup_view.reset();
		m_particle_draw_array.reset();
	}

	HRESULT __fastcall cheat::present(IDXGISwapChain* thisptr, UINT sync_interval, UINT flags)
	{
		rendering::g_context.on_present(thisptr);

		ensure_wnd_proc();

		return m_present.call<HRESULT>(thisptr, sync_interval, flags);
	}

	void cheat::ensure_wnd_proc()
	{
		const auto hwnd = rendering::g_context.get_window();
		if (!hwnd)
			return;

		static unsigned tick = 0;
		if (m_wnd_proc_hwnd == hwnd && m_wnd_proc_orig && ((++tick) & 31u) != 0)
			return;

		if (!IsWindow(hwnd))
			return;

		auto* current = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
		if (current == &wnd_proc)
			return;

		if (m_wnd_proc_hwnd && m_wnd_proc_hwnd != hwnd && m_wnd_proc_orig && IsWindow(m_wnd_proc_hwnd))
			SetWindowLongPtrW(m_wnd_proc_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_wnd_proc_orig));

		m_wnd_proc_orig = current;
		m_wnd_proc_hwnd = hwnd;
		SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wnd_proc));
	}

	HRESULT __fastcall cheat::resize_buffers(IDXGISwapChain* thisptr, UINT buffer_count, UINT width, UINT height, DXGI_FORMAT new_format, UINT swap_chain_flags)
	{
		rendering::g_context.on_resize_buffers();

		const auto result = m_resize_buffers.call<long>(thisptr, buffer_count, width, height, new_format, swap_chain_flags);
		if (SUCCEEDED(result))
		{
			rendering::g_context.on_resize_buffers_post(thisptr);
		}

		return result;
	}

	LRESULT __stdcall cheat::wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_ACTIVATE && LOWORD(wparam) != WA_INACTIVE && rendering::g_menu.is_open() && rendering::g_context.get_window() == hwnd)
		{
			if (addresses::globals::input_system)
			{
				memory::call_vfunc<void>(addresses::globals::input_system, 76, false);
			}

			SetCursor(LoadCursor(nullptr, IDC_ARROW));
			rendering::g_menu.apply_saved_cursor();
		}

		if ((msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) &&
			static_cast<int>(wparam) == settings::g_misc.menu_key.value)
		{
			return 0;
		}

		xui::wndproc(msg, wparam, lparam);
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);

		const bool block_look =
			features::combat::g_misc.antiaim().mouse_override_capturing();

		if (rendering::g_menu.is_open())
		{
			switch (msg)
			{
			case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
			case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
			case WM_MOUSEMOVE:
			case WM_INPUT:
			case WM_KEYDOWN: case WM_KEYUP:
			case WM_SYSKEYDOWN: case WM_SYSKEYUP:
			case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_UNICHAR:
				return 0;
			default:
				break;
			}
		}
		else if (block_look)
		{
			switch (msg)
			{
			case WM_MOUSEMOVE:
			case WM_INPUT:
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
				return 0;
			default:
				break;
			}
		}

		return m_wnd_proc_orig
			? CallWindowProcW(m_wnd_proc_orig, hwnd, msg, wparam, lparam)
			: DefWindowProcW(hwnd, msg, wparam, lparam);
	}

	void __fastcall cheat::cmd_interpreter(std::uintptr_t render_thread, std::uintptr_t item, std::uint8_t flag)
	{
		m_cmd_interpreter.call<void>(render_thread, item, flag);
	}

	void __fastcall cheat::frame_stage_notify(std::uintptr_t thisptr, int stage)
	{
		if (!world_busy() && systems::g_entities.is_empty())
		{
			systems::g_entities.force_update();
		}

		systems::g_local.update();

		const auto features_live = [ ]
		{
			return !world_busy()
				&& systems::g_local.get().is_valid()
				&& systems::g_view.has_camera();
		};

		if (features_live() && stage == 7)
		{
			features::changer::g_agents.on_frame_stage_notify();
			features::world::g_scene.on_frame_stage_notify();
			features::world::g_weather.on_frame_stage_notify();
			features::world::g_ground_particles.on_frame_stage_notify();
			features::world::g_hitkill_particles.on_frame_stage_notify();
			features::world::g_throwable_trail.on_frame_stage_notify();
			particles::update();
			features::misc::g_reveal_radar.on_frame_stage_notify();
			features::misc::g_enemy_spec.on_frame();
			features::misc::g_local_alpha.on_frame_stage_notify();
			features::misc::g_name_changer.on_frame_stage_notify();
			features::misc::g_clantag.on_frame_stage_notify();
			features::misc::g_impacts.on_frame_stage_notify();
		}

		{
			static auto was_active{ false };
			const auto is_active = !world_busy() && settings::g_misc.m_removals.skybox_3d.value;

			if (is_active != was_active)
			{
				if (const auto skybox = CONVAR("r_draw3dskybox"))
				{
					skybox->m_value.i1 = is_active;
				}
				was_active = is_active;
			}
		}

		if (features_live() && stage == 6)
		{
			features::misc::g_dlight.on_frame_stage_notify();
			features::changer::g_gloves.on_frame_stage_notify();
		}

		if (!world_busy() && stage == 6)
			features::visuals::model_preview::on_client_frame();

		m_frame_stage_notify.call<void>(thisptr, stage);

		if (world_busy())
		{
			return;
		}

		if (features_live() && stage == 6)
		{
			features::changer::g_guns.on_frame_stage_notify();
			features::changer::g_knives.on_frame_stage_notify();
			features::changer::g_gloves.on_frame_stage_notify();
		}

		if (stage == 12)
		{
			systems::g_view.update_matrix();
			systems::g_frame_data.update();
		}

		if (features_live() && stage == 6)
		{
			features::combat::g_shared.lc().run();
			features::esp::player::g_chams.bt().update();
			features::esp::player::g_chams.os().update();
			features::esp::player::g_overlay.update_visibility();
			economy::g_tracker.async_heartbeat();
			features::misc::g_scoreboard_weapons.on_frame_stage_notify();
			features::misc::g_killfeed.on_frame_stage_notify();
		}
	}

	void __fastcall cheat::create_move(std::uintptr_t thisptr, int slot, bool active)
	{
		systems::g_local.update();

		if (world_busy())
		{
			return m_create_move.call<void>(thisptr, slot, active);
		}

		const auto eat_input = game_input_blocked();
		const auto local = systems::g_local.get();
		if (!eat_input && local.is_alive && local.team >= 2 && systems::g_view.has_camera() )
		{
			features::movement::g_bhop.pre_create_move(thisptr);
		}

		if (!local.pawn || !local.controller)
		{
			return m_create_move.call<void>(thisptr, slot, active);
		}

		const auto cmd = systems::g_input.get_current_cmd(local.controller);
		if (cmd && systems::g_input.is_subtick_overwrite(cmd))
		{
			systems::g_input.set_weapon_select(cmd, thisptr);
			return;
		}

		m_create_move.call<void>(thisptr, slot, active);

		if (world_busy())
		{
			return;
		}

		if (eat_input)
		{
			const auto live = systems::g_local.get();
			if (live.controller)
			{
				if (const auto current_cmd = systems::g_input.get_current_cmd(live.controller))
				{
					systems::g_input.neutralize(current_cmd);
				}
			}
			return;
		}

		{
			features::combat::g_shared.invalidate_if_needed();
			features::combat::g_legit.invalidate_if_needed();
			features::combat::g_misc.autopeak().reset_if_needed();
		}

		systems::g_local.update();
		const auto live = systems::g_local.get();
		if (!live.is_alive || !memory::is_game_ptr(live.pawn) || !memory::is_game_ptr(live.controller) || !systems::g_view.has_camera())
		{
			return;
		}

		const auto movement_services = reinterpret_cast<C_BasePlayerPawn*>(live.pawn)->m_pMovementServices();
		if (!memory::is_game_ptr(movement_services))
		{
			return;
		}

		const auto last_cmd_processed = reinterpret_cast<CPlayer_MovementServices*>(movement_services)->m_nLastCommandNumberProcessed();
		if (!last_cmd_processed)
		{
			return;
		}

		systems::g_input.update();
		{
			const auto current_cmd = systems::g_input.get();
			if (!current_cmd || !current_cmd->csgo_user_cmd.has_base())
			{
				return;
			}

			systems::g_input.desubtick(current_cmd);
			systems::g_prediction.capture_prestate(live.pawn, movement_services);
			features::movement::g_airstrafe.store_angles();

			const auto freecam_active = features::misc::g_camera.on_create_move(current_cmd);
			if (!freecam_active)
			{
				features::combat::g_shared.update();

				features::combat::g_misc.antiaim().on_create_move(current_cmd);

				features::movement::g_slowwalk.on_create_move(current_cmd);
				features::movement::g_bhop.on_create_move(current_cmd);
				features::movement::g_fastladder.on_create_move(current_cmd);
				features::movement::g_airstrafe.on_create_move(current_cmd);

				features::combat::g_rage.on_create_move(current_cmd);
				features::combat::g_misc.autostop().on_create_move(current_cmd);
				features::combat::g_legit.on_create_move(current_cmd);

				features::combat::g_misc.duckpeek().on_create_move(current_cmd);
				features::movement::g_test_strafer.on_create_move(current_cmd);
				features::misc::g_projectile_trajectory.on_create_move(current_cmd);

				features::combat::g_misc.autopeak().on_create_move(current_cmd);

				features::movement::g_slowwalk.on_create_move(current_cmd);

				const auto final_base = current_cmd->csgo_user_cmd.mutable_base();
				if (final_base && !features::movement::g_test_strafer.handled_this_tick())
				{
					bool has_movement_subtick = false;
					for (int i = 0; i < final_base->subtick_moves_size(); ++i)
					{
						auto* step = final_base->mutable_subtick_moves(i);
						if (step && (step->m_has_bits.test(0x8) || step->m_has_bits.test(0x10)))
						{
							has_movement_subtick = true;
							break;
						}
					}
					if (has_movement_subtick)
					{
						final_base->set_forwardmove(0.0f);
						final_base->set_leftmove(0.0f);
					}
				}
			}

		}
	}

	void __fastcall cheat::handle_view_angles(std::uintptr_t thisptr, int a2)
	{
		const auto view_angles = systems::g_input.get_view_angles();

		m_handle_view_angles.call<void>(thisptr, a2);

		if (world_busy())
		{
			return;
		}

		if (features::combat::g_misc.antiaim().mouse_override_capturing())
		{
			features::combat::g_misc.antiaim().apply_mouse_override_view_freeze();
		}
		else
		{
			systems::g_input.set_view_angles(view_angles);
		}
	}

	void __fastcall cheat::add_entity(std::uintptr_t thisptr, std::uintptr_t entity, std::uint32_t handle)
	{
		systems::g_entities.on_add_entity(entity, handle);

		m_add_entity.call<void>(thisptr, entity, handle);
	}

	void __fastcall cheat::remove_entity(std::uintptr_t thisptr, std::uintptr_t entity, std::uint32_t handle)
	{
		systems::g_entities.on_remove_entity(entity, handle);

		m_remove_entity.call<void>(thisptr, entity, handle);
	}

	void __fastcall cheat::render_view(std::uintptr_t thisptr)
	{
		m_render_view.call<void>(thisptr);

		systems::g_view.update(thisptr + 0x10);
		systems::g_frame_data.update();
	}

	void __fastcall cheat::draw_skybox_array(std::uintptr_t thisptr, std::uintptr_t a2, std::uintptr_t mesh_array, int mesh_count, int a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8)
	{
		const auto patch = !scene_mod_busy();
		if (patch)
		{
			features::world::g_scene.on_draw_skybox_array_pre(mesh_array, mesh_count);
		}

		m_draw_skybox_array.call<void>(thisptr, a2, mesh_array, mesh_count, a5, a6, a7, a8);

		features::world::g_scene.on_draw_skybox_array_post();
	}

	void __fastcall cheat::aggregate_draw_array(std::uintptr_t thisptr, std::uintptr_t a2, std::uintptr_t mesh_array, int mesh_count, int a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8)
	{
		if (!scene_mod_busy() )
		{
			features::world::g_scene.on_draw_scene_object(mesh_array, mesh_count);
		}

		m_aggregate_draw_array.call<void>(thisptr, a2, mesh_array, mesh_count, a5, a6, a7, a8);
	}

	std::uintptr_t __fastcall cheat::light_scene_object(std::uintptr_t thisptr, std::uintptr_t object, std::uintptr_t a3)
	{
		if (!scene_mod_busy())
		{
			features::world::g_scene.on_light_scene_object_pre(object);
			features::misc::g_dlight.apply_scene_color(object);
		}

		const auto result = m_light_scene_object.call<std::uintptr_t>(thisptr, object, a3);

		if (!scene_mod_busy())
		{
			features::world::g_scene.on_light_scene_object_post(object);
		}

		return result;
	}

	void __fastcall cheat::draw_scene_object_array(std::uintptr_t thisptr, std::uintptr_t a2, std::uintptr_t object_array)
	{
		m_draw_scene_object_array.call<void>(thisptr, a2, object_array);

		if (!scene_mod_busy() )
		{
			features::world::g_scene.on_draw_scene_object_array(object_array);
		}
	}

	std::uintptr_t __fastcall cheat::draw_scene_object(std::uintptr_t a1, std::uintptr_t a2, std::uintptr_t batch, int batch_count, int a5, std::uintptr_t a6, std::uintptr_t a7, std::uintptr_t a8)
	{
		if (!scene_mod_busy() )
		{
			features::world::g_scene.on_draw_scene_object(batch, batch_count);
		}

		return m_draw_scene_object.call<std::uintptr_t>(a1, a2, batch, batch_count, a5, a6, a7, a8);
	}

	std::uint8_t __fastcall cheat::world_material_swap_active()
	{
		if (scene_mod_busy())
		{
			return 0;
		}

		return features::world::scene::world_material_swap_active() ? 1 : 0;
	}

	std::uintptr_t __fastcall cheat::force_world_material_rcx(std::uintptr_t mesh_entry)
	{
		if (scene_mod_busy() || !memory::is_game_ptr(mesh_entry))
		{
			return 0;
		}

		return features::world::g_scene.force_world_material_rcx(mesh_entry);
	}

	bool __fastcall cheat::is_glowing(std::uintptr_t glow_property)
	{
		if (glow_property && !scene_mod_busy() )
		{
			const auto owner_entity = memory::read<std::uintptr_t>(glow_property + 0x18);
			if (memory::is_game_ptr(owner_entity))
			{
				const auto owner_hash = fnv1a::runtime_hash(systems::g_entities.get_schema_name(owner_entity));
				if (owner_hash)
				{
					if (features::esp::player::g_glow.on_is_glowing(owner_entity, owner_hash))
					{
						return true;
					}

					if (features::esp::item::g_glow.on_is_glowing(owner_entity, owner_hash))
					{
						return true;
					}
				}
			}
		}

		return m_is_glowing.call<bool>(glow_property);
	}

	void __fastcall cheat::get_glow_color(std::uintptr_t glow_property, float* color)
	{
		if (glow_property && !scene_mod_busy() )
		{
			const auto owner_entity = memory::read<std::uintptr_t>(glow_property + 0x18);
			if (memory::is_game_ptr(owner_entity))
			{
				const auto owner_hash = fnv1a::runtime_hash(systems::g_entities.get_schema_name(owner_entity));
				if (owner_hash)
				{
					if (features::esp::player::g_glow.on_get_glow_color(owner_entity, owner_hash, color))
					{
						return;
					}

					if (features::esp::item::g_glow.on_get_glow_color(owner_entity, owner_hash, color))
					{
						return;
					}
				}
			}
		}

		m_get_glow_color.call<void>(glow_property, color);
	}

	void __fastcall cheat::generate_primitives(std::uintptr_t thisptr, std::uintptr_t scene_object, std::uintptr_t scene_view, std::uintptr_t primitive_buffer)
	{
		if (scene_mod_busy())
		{
			m_generate_primitives.call<void>(thisptr, scene_object, scene_view, primitive_buffer);
			return;
		}

		if (scene_object && !memory::is_game_ptr(scene_object))
			return;

		if (scene_object && features::esp::detail::scene_object_freed(scene_object))
			return;

		std::uintptr_t owner_entity = 0;
		std::uint32_t owner_hash = 0;
		std::uint32_t owner_handle = 0;
		bool skip_original = false;

		if (scene_object)
		{
			owner_handle = memory::read<std::uint32_t>(scene_object + 0xc0);
			if (owner_handle == features::esp::detail::k_managed_scene_owner)
			{
				if (!features::esp::detail::scene_object_live(scene_object))
					return;

				m_generate_primitives.call<void>(thisptr, scene_object, scene_view, primitive_buffer);

				const auto buffer = features::esp::detail::read_primitive_buffer(primitive_buffer);
				if (buffer)
				{
					const auto n = buffer->count();
					for (auto i = 0; i < n; ++i)
					{
						const auto primitive = buffer->at(i);
						if (primitive)
							memory::write<float>(primitive + features::esp::detail::primitive_opacity_offset, 0.f);
					}
				}
				return;
			}

			if (owner_handle &&
				owner_handle != 0xffffffffu &&
				owner_handle != 0xfffffffeu &&
				chams_generate_needed())
			{
				owner_entity = systems::g_entities.lookup(owner_handle);
				if (owner_entity)
				{
					const auto schema_name = systems::g_entities.get_schema_name(owner_entity);
					if (features::esp::detail::is_chams_schema(schema_name))
					{
						owner_hash = fnv1a::runtime_hash(schema_name);
						if (owner_hash &&
							features::esp::player::g_chams.on_generate_primitives(
								owner_entity,
								owner_hash,
								scene_object,
								primitive_buffer,
								m_generate_primitives.original<void(__fastcall*)(std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t)>(),
								thisptr,
								scene_view))
						{
							skip_original = true;
						}
					}
				}
			}
		}

		if (!skip_original)
			m_generate_primitives.call<void>(thisptr, scene_object, scene_view, primitive_buffer);

		if (owner_entity && owner_hash)
			features::visuals::model_preview::on_generate_primitives(owner_entity, owner_hash, scene_object, owner_handle);
	}

	std::uint32_t __fastcall cheat::get_weapon_type(std::uintptr_t entity)
	{
		if (!memory::is_game_ptr(entity))
		{
			return 7u;
		}

		const auto vdata = memory::read<std::uintptr_t>(
			entity + SCHEMA_OFFSET("C_BaseEntity", "m_nSubclassID"_hash) + 0x8);
		if (!memory::is_game_ptr(vdata))
		{
			return 7u;
		}

		return m_get_weapon_type.call<std::uint32_t>(entity);
	}

	ID3D11ShaderResourceView* __fastcall cheat::get_resource_view(void* texture_manager, int** texture, char a3, char a4, const char* a5)
	{
		using fn_t = ID3D11ShaderResourceView * (__fastcall*)(void*, int**, char, char, const char*);
		if (world_busy())
		{
			return m_get_resource_view.call<ID3D11ShaderResourceView*>(texture_manager, texture, a3, a4, a5);
		}

		return features::visuals::model_preview::on_get_resource_view(
			texture_manager,
			texture,
			a3,
			a4,
			a5,
			m_get_resource_view.original<fn_t>());
	}

	std::uintptr_t __fastcall cheat::parse_report_hit(std::uintptr_t thisptr, std::uint8_t deleting)
	{

		if (!world_busy())
		{
			features::misc::g_impacts.on_report_hit(thisptr);
		}

		return m_parse_report_hit.call<std::uintptr_t>(thisptr, deleting);
	}

	std::uintptr_t __fastcall cheat::setup_fog(__m128i* output, int* mode)
	{
		if (!scene_mod_busy() && features::world::g_scene.on_setup_fog(output, mode))
		{
			return 0;
		}

		return m_setup_fog.call<std::uintptr_t>(output, mode);
	}

	std::uintptr_t __fastcall cheat::set_shader_param(__m128i* map, std::uint32_t hash, __m128i* value)
	{
		if (!scene_mod_busy())
		{
			features::world::g_scene.on_set_shader_param(value, hash);
		}

		return m_set_shader_param.call<std::uintptr_t>(map, hash, value);
	}

	std::uintptr_t __fastcall cheat::set_postprocess_vec(__m128i* map, std::uint32_t hash, __m128i* value)
	{
		constexpr std::uint32_t dof_ranges{ 0x2ACAB07C };

		if (!scene_mod_busy() && settings::g_world.m_scene.dof.value && hash != dof_ranges)
		{
			__m128i* dof_value{};
			features::world::g_scene.on_set_shader_param(dof_value, dof_ranges);
			if (dof_value)
			{
				m_set_postprocess_vec.call<std::uintptr_t>(map, dof_ranges, dof_value);
			}
		}

		if (!scene_mod_busy())
		{
			features::world::g_scene.on_set_shader_param(value, hash);
		}
		return m_set_postprocess_vec.call<std::uintptr_t>(map, hash, value);
	}

	void __fastcall cheat::override_view(std::uintptr_t thisptr, std::uintptr_t view_setup)
	{
		m_override_view.call<void>(thisptr, view_setup);

		if (world_busy())
		{
			return;
		}

		features::misc::g_camera.on_override_view(view_setup);
		features::misc::g_removals.on_override_view(view_setup);
		features::combat::g_misc.duckpeek().on_override_view(view_setup);
	}

	void __fastcall cheat::update_fov_sensitivity(std::uintptr_t thisptr)
	{
		m_update_fov_sensitivity.call<void>(thisptr);

		if (!world_busy())
		{
			features::misc::g_camera.update_fov_sensitivity(thisptr);
		}
	}

	void __fastcall cheat::render_scope(std::uintptr_t a1, std::uintptr_t a2)
	{
		m_render_scope.call<void>(a1, a2);

		if (!world_busy() && settings::g_misc.m_removals.scope.value)
		{
			memory::write<std::uint8_t>(a2 + 4, 0);
		}
	}

	namespace {

		[[nodiscard]] bool pawn_is_scoped( std::uintptr_t pawn )
		{
			if ( !memory::is_game_ptr( pawn ) || !systems::g_entities.is_cs_player_pawn( pawn ) )
			{
				return false;
			}

			return reinterpret_cast< C_CSPlayerPawn* >( pawn )->m_bIsScoped( );
		}

	}

	bool __fastcall cheat::render_crosshair(std::uintptr_t a1)
	{
		if (settings::g_misc.m_removals.force_crosshair.value && !world_busy())
		{
			if ( pawn_is_scoped( systems::g_local.get( ).pawn ) )
			{
				return false;
			}
			return true;
		}

		return m_render_crosshair.call<bool>(a1);
	}

	bool __fastcall cheat::weapon_hides_crosshair(std::uintptr_t zoom_data)
	{
		if (settings::g_misc.m_removals.force_crosshair.value && !world_busy())
		{
			return pawn_is_scoped( systems::g_local.get( ).pawn );
		}

		return m_weapon_hides_crosshair.call<bool>(zoom_data);
	}

	float __fastcall cheat::prepare_scene_material(std::uintptr_t material, void* a2, float a3)
	{
		if (material && !scene_mod_busy())
		{
			features::misc::g_removals.on_prepare_scene_material(material);
		}

		return m_prepare_scene_material.call<float>(material, a2, a3);
	}

	void __fastcall cheat::post_network_data_received(std::uintptr_t thisptr)
	{
		m_post_network_data_received.call<void>(thisptr);
	}

	bool __fastcall cheat::draw_overhead(std::uintptr_t pawn, std::uint32_t player_slot)
	{
		if (!world_busy() && settings::g_misc.m_removals.overhead.value && pawn == systems::g_local.get().pawn)
		{
			return false;
		}

		return m_draw_overhead.call<bool>(pawn, player_slot);
	}

	std::uintptr_t __fastcall cheat::draw_legs(std::uintptr_t a1, std::uintptr_t a2, std::uintptr_t a3, std::uintptr_t a4, std::uintptr_t a5)
	{
		if (settings::g_misc.m_removals.legs.value && !world_busy())
		{
			return 0;
		}

		return m_draw_legs.call<std::uintptr_t>(a1, a2, a3, a4, a5);
	}

	bool __fastcall cheat::get_transforms_for_hitbox_list(std::uintptr_t a1, std::uintptr_t a2, int* a3)
	{
		if (world_busy() || !features::combat::g_shared.autowalling())
		{
			return m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);
		}

		const auto record = features::combat::g_shared.current_autowall_record();
		if (!record || !record->valid)
		{
			return m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);
		}

		const auto a3_addr = reinterpret_cast<std::uintptr_t>(a3);
		const auto count = a3_addr ? memory::read<int>(a3_addr) : 0;
		const auto shape_array = a3_addr ? memory::read<std::uintptr_t>(a3_addr + 8) : 0;
		const auto entity_bone_cache = memory::is_game_ptr(a1) ? memory::read<std::uintptr_t>(a1 + 0x1c0) : 0;
		const auto model_handle = memory::is_game_ptr(a1) ? memory::read<std::uintptr_t>(a1 + 0x1e0) : 0;
		const auto model = model_handle ? memory::read<std::uintptr_t>(model_handle) : 0;

		if (count <= 0 || count > 256 || !shape_array || !entity_bone_cache || !model)
		{
			return m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);
		}

		static const auto get_bone_index = PATTERN(PATTERN_GET_BONE_INDEX);
		if (!get_bone_index)
		{
			return m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);
		}

		for (auto i = 0; i < count; ++i)
		{
			const auto shape_ptr = shape_array + 16ull * i;
			const auto bone_index = memory::call<int>(get_bone_index, model, shape_ptr);

			if (bone_index >= 256)
			{
				return m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);
			}
		}

		const auto target_scene = record->game_scene_node ? record->game_scene_node : reinterpret_cast<C_BaseEntity*>(record->pawn)->m_pGameSceneNode();
		const auto target_bone_cache = target_scene
			? memory::read<std::uintptr_t>(target_scene + SCHEMA_OFFSET("CSkeletonInstance", "m_modelState"_hash) + 0x80)
			: 0;
		const auto result = m_get_transforms_for_hitbox_list.call<bool>(a1, a2, a3);

		if (!result)
		{
			return false;
		}

		const auto same_scene_object = target_scene && a1 == target_scene;
		const auto same_bone_cache =
			memory::detail::is_user_addr(entity_bone_cache) &&
			memory::detail::is_user_addr(target_bone_cache) &&
			entity_bone_cache == target_bone_cache;

		if (!same_scene_object && !same_bone_cache)
		{
			return result;
		}

		const auto output_array = a2 ? memory::read<std::uintptr_t>(a2 + 16) : 0;

		if (!output_array || count <= 0)
		{
			return result;
		}

		for (auto i = 0; i < count; ++i)
		{
			const auto shape_ptr = shape_array + 16ull * i;
			const auto bone_index = memory::call<int>(get_bone_index, model, shape_ptr);

			if (bone_index < 0 || bone_index >= record->bone_count)
			{
				continue;
			}

			const auto dst = output_array + 32ull * i;
			std::memcpy(reinterpret_cast<void*>(dst), &record->bones[bone_index], 32);
		}

		return true;
	}

	void __fastcall cheat::sort_primitives(std::uintptr_t thisptr, std::uintptr_t a2, std::uintptr_t a3, std::uint32_t a4)
	{
		m_sort_primitives.call<void>(thisptr, a2, a3, a4);

		if (!world_busy() )
		{
			features::esp::player::g_chams.on_sort_primitives(a3, a4);
		}
	}

	float __fastcall cheat::get_inaccuracy(std::uintptr_t thisptr, float* a2, float* a3)
	{
		const auto result = m_get_inaccuracy.call<float>(thisptr, a2, a3);

#if defined(__clang__) || defined(__GNUC__)
		const auto result_address = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
#else
		const auto result_address = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
#endif

		static const auto base_fire_guns_get_inaccuracy = PATTERN(PATTERN_BASE_FIRE_GUNS_GET_INACCURACY);
		if (!world_busy() &&
			base_fire_guns_get_inaccuracy &&
			result_address > base_fire_guns_get_inaccuracy &&
			result_address < base_fire_guns_get_inaccuracy + 0x600)
		{
			features::misc::g_impacts.on_base_fire_guns_get_inaccuracy(thisptr, result);
		}

		return result;
	}

	float* __fastcall cheat::get_interpolated_shoot_position(std::uintptr_t thisptr, float* out, int* tick_frac)
	{
		const auto result = m_get_interpolated_shoot_position.call<float*>(thisptr, out, tick_frac);

		if (!world_busy() && features::combat::g_rage.is_firing_this_tick())
		{
			features::misc::g_impacts.on_get_interpolated_shoot_position(thisptr, out);
		}

		return result;
	}

	std::uintptr_t __fastcall cheat::level_initialization(std::uintptr_t a1, const char* new_map)
	{
		systems::g_lifecycle.begin();
		drop_world_runtime(false);
		features::changer::custom_paint::set_map_name(new_map);

		if (new_map && new_map[0])
		{
			const char* leaf = std::strrchr(new_map, '/');
			rendering::g_widgets.s_map_name = leaf ? leaf + 1 : new_map;
		}

		const auto result = m_level_initialization.call<std::uintptr_t>(a1, new_map);
		systems::g_entities.force_update();
		systems::g_lifecycle.end();
		return result;
	}

	std::uintptr_t __fastcall cheat::level_shutdown(std::uintptr_t a1)
	{
		systems::g_lifecycle.begin();
		drop_world_runtime(true);
		return m_level_shutdown.call<std::uintptr_t>(a1);
	}

	void __fastcall cheat::read_frame_input(std::uintptr_t a1, std::uint32_t a2)
	{
		m_read_frame_input.call<void>(a1, a2);
	}

	void __fastcall cheat::process_input_event(std::uintptr_t csgo_input, int slot, float frametime)
	{
		if (game_input_blocked())
		{
			return;
		}

		systems::g_legit_input.on_process_input_event(csgo_input, slot);
		m_process_input_event.call<void>(csgo_input, slot, frametime);
	}

	std::uintptr_t __fastcall cheat::render_decals(std::uintptr_t render_context, std::uintptr_t** render_view, bool pass_flag_a, bool pass_flag_b)
	{
		if (settings::g_misc.m_removals.decals.value)
		{
			return 0;
		}

		return m_render_decals.call<std::uintptr_t>(render_context, render_view, pass_flag_a, pass_flag_b);
	}

	void __fastcall cheat::render_smoke(std::uintptr_t a1, std::uintptr_t a2, int a3, int a4, std::uintptr_t a5, std::uintptr_t a6)
	{
		static std::once_flag alright;
	std::call_once(alright, [&]
			{
				if (!a2)
				{
					return;
				}

				if (m_render_smoke_map.create(reinterpret_cast<void*>(memory::get_vfunc(a2, 32)), &render_smoke_map))
				{
					m_render_smoke_map.enable();
				}

				if (m_render_smoke_unmap.create(reinterpret_cast<void*>(memory::get_vfunc(a2, 33)), &render_smoke_unmap))
				{
					m_render_smoke_unmap.enable();
				}
			});

		if (settings::g_misc.m_removals.smoke.value)
		{
			return;
		}

		m_render_smoke.call<void>(a1, a2, a3, a4, a5, a6);
	}

	std::uintptr_t __fastcall cheat::render_smoke_map(std::uintptr_t thisptr, std::size_t size, std::uintptr_t* out_ptr)
	{
		const auto result = m_render_smoke_map.call<std::uintptr_t>(thisptr, size, out_ptr);

		if (!world_busy())
		{
			features::world::g_smoke.on_map(result, size, out_ptr && *out_ptr ? *out_ptr : 0);
		}

		return result;
	}

	void __fastcall cheat::render_smoke_unmap(std::uintptr_t thisptr, std::uintptr_t ctx, std::size_t size)
	{
		if (!world_busy())
		{
			features::world::g_smoke.on_unmap(ctx);
		}
		m_render_smoke_unmap.call<void>(thisptr, ctx, size);
	}

	char __fastcall cheat::set_info(std::uintptr_t rcx, std::uintptr_t a2)
	{
		if (world_busy())
		{
			return m_set_info.call<char>(rcx, a2);
		}

		const auto& cfg = settings::g_misc.m_name_changer;
		const auto should_override = cfg.override_name.value || features::misc::name_changer::s_name_change_pending;
		if (should_override && a2)
		{
			const auto arg_list = memory::read<std::uintptr_t>(a2 + 0x440);
			const auto key = arg_list
				? memory::read<const char*>(arg_list + 0x8)
				: nullptr;

			if (key && _stricmp(key, "name") == 0)
			{
				constexpr std::uint64_t fcvar_protected = 1ull << 5;
				constexpr std::uint64_t fcvar_userinfo = 1ull << 9;
				constexpr std::uint64_t fcvar_registry_restricted = 1ull << 10;

				if (addresses::globals::cvar)
				{
					if (const auto name_cvar = addresses::globals::cvar->find("name"_hash))
					{
						const auto flags_address = reinterpret_cast<std::uintptr_t>(name_cvar) + offsetof(c_convar, m_flags);
						const auto flags = memory::read<std::uint64_t>(flags_address);
						memory::write<std::uint64_t>(flags_address,
							(flags | fcvar_userinfo) & ~(fcvar_protected | fcvar_registry_restricted));
					}
				}

				const auto& display = features::misc::name_changer::s_display_name;
				if (!display.empty())
				{
					static char name_buf[128]{};
					std::memset(name_buf, 0, sizeof(name_buf));
					std::snprintf(name_buf, sizeof(name_buf), "%s", display.c_str());
					memory::write<const char*>(arg_list + 0x10, name_buf);
				}
			}
		}

		if (features::misc::g_clantag.on_set_info(a2))
		{
			return m_set_info.call<char>(rcx, a2);
		}

		return m_set_info.call<char>(rcx, a2);
	}

	void __fastcall cheat::viewmodel_update_and_setup_view(std::uintptr_t viewmodel, void* position, void* angles, bool update)
	{
		const auto pos = reinterpret_cast<math::vector3*>(position);
		const auto ang = reinterpret_cast<math::vector3*>(angles);

		if (!settings::g_misc.m_viewmodel_adjust.enabled.value || !pos || !ang || world_busy())
		{
			m_viewmodel_update_and_setup_view.call<void>(viewmodel, position, angles, update);
			return;
		}

		auto adjusted_position = *pos;
		auto adjusted_angles = *ang;
		features::misc::g_viewmodel.apply(&adjusted_position, &adjusted_angles);
		m_viewmodel_update_and_setup_view.call<void>(viewmodel, &adjusted_position, &adjusted_angles, update);
	}

	void __fastcall cheat::draw_flash_effect(std::uintptr_t a1, int a2, std::uintptr_t* a3, std::uintptr_t a4, __m128* a5)
	{
		if (settings::g_misc.m_removals.flash_alpha.value < 100.0f && settings::g_misc.m_removals.flash_alpha.value != 0.0f && !world_busy())
		{
			const auto view_pawn = systems::g_local.get().view_pawn();
			if (memory::is_game_ptr(view_pawn) && systems::g_entities.is_player_pawn_base(view_pawn))
			{
				const auto max = settings::g_misc.m_removals.flash_alpha.value / 100.0f * 255.0f;
				reinterpret_cast<C_CSPlayerPawnBase*>(view_pawn)->m_flFlashMaxAlpha() = max;
			}
		}

		if (settings::g_misc.m_removals.flash_alpha.value != 0.0f)
		{
			m_draw_flash_effect.call<void>(a1, a2, a3, a4, a5);
		}
	}

	void* __fastcall cheat::particle_draw_array(std::int64_t a1, std::int64_t a2, __m128* a3, unsigned long long* a4, void* a5)
	{
		if (!world_busy())
		{
			features::world::particle_modulation::apply(reinterpret_cast<void*>(a2));
		}
		return m_particle_draw_array.call<void*>(a1, a2, a3, a4, a5);
	}

}


