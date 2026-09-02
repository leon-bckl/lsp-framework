#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace lsp{

/*
 * Wraps a callable with bound arguments.
 * Avoids allocation if callable is small enough.
 * Can only be run once.
 */
template<typename R>
class TaskFunction final{
public:
	using ResultType = R;

	TaskFunction() = default;
	TaskFunction(std::nullptr_t){}

	template<typename F, typename... Args>
	requires std::is_invocable_r_v<R, std::decay_t<F>, std::decay_t<Args>...> &&
					 std::is_nothrow_move_constructible_v<std::decay_t<F>> &&
					 (std::is_nothrow_move_constructible_v<std::decay_t<Args>> && ...) &&
					 (std::constructible_from<std::decay_t<Args>, Args&&> && ...)
	TaskFunction(F&& f, Args&&... args)
	{
		using ImplType = Impl<std::decay_t<F>, std::decay_t<Args>...>;

		static_assert(
			alignof(ImplType) <= alignof(std::max_align_t),
			"TaskFunction callable must have alignment <= alignof(max_align_t)");

		void* mem;

		if(sizeof(ImplType) > StorageSize)
		{
			m_storage.heap        = operator new(sizeof(ImplType));
			m_storage.isAllocated = true;
			mem                   = m_storage.heap;
		}
		else
		{
			mem = m_storage.local;
		}

		new(mem) ImplType(std::forward<F>(f), std::forward<Args>(args)...);
		m_storage.isValid = true;
	}

	~TaskFunction()
	{
		deinit();
	}

	explicit operator bool() const
	{
		return m_storage.isValid;
	}

	auto run() && -> ResultType
	{
		assert(*this);
		auto tmp = std::move(*this); // Makes sure impl is destructed after calling run
		return tmp.impl()->run();
	}

	auto operator()() && -> ResultType
	{
		return std::move(*this).run();
	}

	TaskFunction(const TaskFunction&) = delete;
	TaskFunction& operator=(const TaskFunction&) = delete;

	TaskFunction(TaskFunction&& other) noexcept
	{
		*this = std::move(other);
	}

	TaskFunction& operator=(TaskFunction&& other) noexcept
	{
		deinit();

		if(other.m_storage.isValid)
		{
			if(other.m_storage.isAllocated)
			{
				m_storage.heap        = std::exchange(other.m_storage.heap, nullptr);
				m_storage.isAllocated = std::exchange(other.m_storage.isAllocated, false);
			}
			else
			{
				other.impl()->moveTo(m_storage.local);
				other.impl()->~ImplInterface();
			}

			m_storage.isValid = std::exchange(other.m_storage.isValid, false);
		}

		return *this;
	}

private:
	static constexpr auto StorageSize = sizeof(void*) * 4;

	struct Storage{
		union alignas(std::max_align_t){
			void*     heap;
			std::byte local[StorageSize];
		};

		bool isAllocated = false;
		bool isValid     = false;
	};

	struct ImplInterface{
		virtual ~ImplInterface() = default;
		virtual auto run() -> ResultType        = 0;
		virtual void moveTo(void* mem) noexcept = 0;
	};

	template<typename F, typename... Args>
	struct Impl final : ImplInterface{
		F                   func;
		[[no_unique_address]]
		std::tuple<Args...> storedArgs;

		Impl(F&& f, Args... args)
			: func(std::forward<F>(f))
			, storedArgs(std::move(args)...)
		{
		}

		auto run() -> ResultType override
		{
			return static_cast<ResultType>(std::apply([this](auto&&... args) -> ResultType
			{
				return static_cast<ResultType>(std::invoke(std::move(func), std::forward<decltype(args)>(args)...));
			}, std::move(storedArgs)));
		}

		void moveTo(void* mem) noexcept override
		{
			new(mem) Impl(std::move(*this));
		}
	};

	auto impl() -> ImplInterface*
	{
		if(!m_storage.isValid)
			return nullptr;

		if(m_storage.isAllocated)
			return static_cast<ImplInterface*>(m_storage.heap);

		return std::launder(reinterpret_cast<ImplInterface*>(m_storage.local));
	}

	void deinit()
	{
		if(m_storage.isValid)
		{
			impl()->~ImplInterface();
			m_storage.isValid = false;
		}

		if(m_storage.isAllocated)
		{
			operator delete(m_storage.heap);
			m_storage.isAllocated = false;
		}
	}

	Storage m_storage;
};

} // namespace lsp
