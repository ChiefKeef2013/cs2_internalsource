#pragma once

namespace threadpool {

	enum class job_priority : std::uint8_t
	{
		low = 0,
		normal = 1,
		high = 2,
		urgent = 3,
	};

	enum class job_status : int
	{
		idle = 0,
		queued = 1,
		running = 2,
		aborted = 3,
		reset = 4,
	};

	class job
	{
	public:
		job( ) = default;
		explicit job( std::uintptr_t ptr, bool add_reference = true );
		~job( );

		job( const job& o );
		job& operator=( const job& o );
		job( job&& o ) noexcept;
		job& operator=( job&& o ) noexcept;

		[[nodiscard]] bool valid( ) const { return this->m_ptr != 0; }
		[[nodiscard]] std::uintptr_t get( ) const { return this->m_ptr; }
		[[nodiscard]] explicit operator bool( ) const { return this->valid( ); }

		[[nodiscard]] job_status status( ) const;
		[[nodiscard]] bool complete( ) const;
		void wait( ) const;

	private:
		void add_ref( ) const;
		void do_release( ) const;

		std::uintptr_t m_ptr{};
	};

	class pool
	{
	public:
		pool( ) = default;
		explicit pool( std::uintptr_t instance ) : m_instance( instance ) {}

		[[nodiscard]] bool valid( ) const { return this->m_instance != 0; }
		[[nodiscard]] std::uintptr_t get( ) const { return this->m_instance; }

		void add_job( std::uintptr_t job ) const;

	private:
		std::uintptr_t m_instance{};
	};

	bool initialize( );

	std::uintptr_t make_job( std::function<void( )>&& func, job_priority priority = job_priority::normal, const char* debug_name = nullptr );
	job run( std::function<void( )> func, job_priority priority = job_priority::normal );
	void run_sync( std::function<void( )> func, job_priority priority = job_priority::normal );
	void parallel_for( int begin, int end, const std::function<void( int, int )>& body, int min_chunk_size = 1, job_priority priority = job_priority::normal );
	void run_batch( std::span<std::function<void( )>> tasks, job_priority priority = job_priority::normal );

	namespace detail {

		namespace offsets {

			constexpr std::size_t refcount {0x08};
			constexpr std::size_t status {0x10};
			constexpr std::size_t flags_word {0x14};
			constexpr std::size_t priority {0x15};
			constexpr std::size_t name_set_flag {0x16};
			constexpr std::size_t pool_owner {0x18};
			constexpr std::size_t completion_event {0x20};
			constexpr std::size_t completion_counter {0x28};
			constexpr std::size_t name_buffer {0x30};
			constexpr std::size_t sbo_buffer {0x50};
			constexpr std::size_t callable_impl {0x88};

		}

		constexpr std::size_t k_job_alloc_size {144};
		constexpr std::size_t k_name_buffer_len {31};
		constexpr std::size_t k_sbo_buffer_size {56};
		constexpr std::size_t k_vti_add_job {16};

		inline std::uintptr_t std_function_job_vtable{ 0 };
		inline pool g_pool{};

	}

	inline job::job (std::uintptr_t ptr, bool add_reference) : m_ptr (ptr) {
		if (this->m_ptr && add_reference) {
			this->add_ref ();
		}
	}

	inline job::~job () {
		if (this->m_ptr) {
			this->do_release ();
		}
	}

	inline job::job (const job& o) : m_ptr (o.m_ptr) {
		if (this->m_ptr) {
			this->add_ref ();
		}
	}

	inline job& job::operator=(const job& o) {
		if (this != &o) {
			if (this->m_ptr) {
				this->do_release ();
			}

			this->m_ptr = o.m_ptr;

			if (this->m_ptr) {
				this->add_ref ();
			}
		}

		return *this;
	}

	inline job::job (job&& o) noexcept : m_ptr (o.m_ptr) {
		o.m_ptr = 0;
	}

	inline job& job::operator=(job&& o) noexcept {
		if (this != &o) {
			if (this->m_ptr) {
				this->do_release ();
			}

			this->m_ptr = o.m_ptr;
			o.m_ptr = 0;
		}

		return *this;
	}

	inline job_status job::status () const {
		return static_cast<job_status>(*reinterpret_cast<volatile int*>(this->m_ptr + detail::offsets::status));
	}

	inline bool job::complete () const {
		return this->status () == job_status::idle;
	}

	inline void job::wait () const {
		if (!this->m_ptr) {
			return;
		}

		auto event_pp = *reinterpret_cast<HANDLE**>(this->m_ptr + detail::offsets::completion_event);
		if (event_pp && *event_pp) {
			WaitForSingleObject (*event_pp, INFINITE);
			return;
		}

		auto counter = *reinterpret_cast<volatile int**>(this->m_ptr + detail::offsets::completion_counter);
		if (counter) {
			while (this->status () != job_status::idle) {
				auto snapshot {*counter};
				WaitOnAddress ((volatile void*) counter, &snapshot, sizeof (int), 1);
			}

			return;
		}

		while (!this->complete ()) {
			_mm_pause ();
		}
	}

	inline void job::add_ref () const {
		memory::call_vfunc<long> (this->m_ptr, 1);
	}

	inline void job::do_release () const {
		memory::call_vfunc<long> (this->m_ptr, 2);
	}

	inline void pool::add_job (std::uintptr_t j) const {
		memory::call_vfunc<void> (this->m_instance, detail::k_vti_add_job, j);
	}

	inline std::uintptr_t make_job (std::function<void ()>&& func, job_priority priority, const char* debug_name) {
		auto raw = reinterpret_cast<std::uintptr_t>(new std::uint8_t [detail::k_job_alloc_size] ());
		if (!raw) {
			return 0;
		}

		*reinterpret_cast<std::uintptr_t*>(raw) = detail::std_function_job_vtable;
		*reinterpret_cast<std::int32_t*>(raw + detail::offsets::refcount) = 1;
		*reinterpret_cast<volatile int*>(raw + detail::offsets::status) = static_cast<int>(job_status::reset);
		*reinterpret_cast<std::uint8_t*>(raw + detail::offsets::priority) = static_cast<std::uint8_t>(priority);

		auto dest_sbo = reinterpret_cast<void*>(raw + detail::offsets::sbo_buffer);
		auto dest_impl = reinterpret_cast<std::uintptr_t*>(raw + detail::offsets::callable_impl);

		alignas(16) std::uint8_t temp [sizeof (std::function<void ()>)] {};
		auto fn = new (temp) std::function<void ()> (std::move (func));

		auto src_sbo = reinterpret_cast<std::uint8_t*>(fn);
		auto src_impl = *reinterpret_cast<std::uintptr_t*>(src_sbo + detail::k_sbo_buffer_size);

		if (src_impl) {
			auto src_addr = reinterpret_cast<std::uintptr_t>(src_sbo);
			auto is_sbo = (src_impl >= src_addr && src_impl < src_addr + detail::k_sbo_buffer_size);

			std::memcpy (dest_sbo, src_sbo, detail::k_sbo_buffer_size);

			if (is_sbo) {
				*dest_impl = reinterpret_cast<std::uintptr_t> (dest_sbo) + (src_impl - src_addr);
			} else {
				*dest_impl = src_impl;
			}

			*reinterpret_cast<std::uintptr_t*> (src_sbo + detail::k_sbo_buffer_size) = 0;
		}

		fn->~function ();

		if (debug_name) {
			auto dst = reinterpret_cast<char*>(raw + detail::offsets::name_buffer);
			std::size_t i {0};

			while (i < detail::k_name_buffer_len - 1 && debug_name [i]) {
				dst [i] = debug_name [i];
				++i;
			}

			dst [i] = '\0';

			*reinterpret_cast<std::uint8_t*> (raw + detail::offsets::name_set_flag) = 1;
		}

		return raw;
	}

	inline bool initialize () {
		const auto tier0 = MODULE_BASE ("tier0.dll");
		if (!tier0) {
			return false;
		}

		detail::g_pool = pool (memory::read<std::uintptr_t> (MODULE_EXPORT ("tier0.dll:g_pThreadPool")));
		if (!detail::g_pool.get ()) {
			return false;
		}

		detail::std_function_job_vtable = memory::find_vtable_by_rtti (tier0, xs ("CStdFunctionJob"));
		if (!detail::std_function_job_vtable) {
			return false;
		}

		return true;
	}

	inline job run (std::function<void ()> func, job_priority priority) {
		auto raw = make_job (std::move (func), priority);
		if (!raw) {
			return {};
		}

		job handle (raw, false);
		detail::g_pool.add_job (raw);
		return handle;
	}

	inline void run_sync (std::function<void ()> func, job_priority priority) {
		auto handle = run (std::move (func), priority);
		if (handle) {
			handle.wait ();
		}
	}

	inline void parallel_for (int begin, int end, const std::function<void (int, int)>& body, int min_chunk_size, job_priority priority) {
		if (begin >= end) {
			return;
		}

		const int total {end - begin};
		const int max_chunks {std::max (static_cast<int>(std::thread::hardware_concurrency ()), 1)};

		auto chunk_size {(total + max_chunks - 1) / max_chunks};
		if (chunk_size < min_chunk_size) {
			chunk_size = min_chunk_size;
		}

		const auto num_chunks {(total + chunk_size - 1) / chunk_size};

		if (num_chunks <= 1) {
			body (begin, end);
			return;
		}

		std::vector<job> jobs;
		jobs.reserve (static_cast<std::size_t>(num_chunks) - 1);

		for (auto i = 0; i < num_chunks - 1; ++i) {
			const auto cb {begin + i * chunk_size};
			const auto ce {std::min (cb + chunk_size, end)};

			jobs.push_back (run ([&body, cb, ce] () { body (cb, ce); }, priority));
		}

		body (begin + (num_chunks - 1) * chunk_size, end);

		for (auto& j : jobs) {
			j.wait ();
		}
	}

	inline void run_batch (std::span<std::function<void ()>> tasks, job_priority priority) {
		if (tasks.empty ()) {
			return;
		}

		std::vector<job> jobs;
		jobs.reserve (tasks.size ());

		for (auto& task : tasks) {
			jobs.push_back (run (std::move (task), priority));
		}

		for (auto& j : jobs) {
			j.wait ();
		}
	}

}
