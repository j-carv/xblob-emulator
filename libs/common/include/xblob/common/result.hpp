#pragma once

#include "xblob/common/error.hpp"

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

namespace xblob {

template <typename T, typename E = Error>
class Result {
public:
    using value_type = T;
    using error_type = E;

    // Constructors for success
    Result(const T& val) : storage_(val) {}
    Result(T&& val) : storage_(std::move(val)) {}

    // Constructors for error
    Result(const E& err) : storage_(err) {}
    Result(E&& err) : storage_(std::move(err)) {}

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const T& value() const& {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] T& value() & {
        assert(has_value());
        return std::get<T>(storage_);
    }

    [[nodiscard]] T&& value() && {
        assert(has_value());
        return std::get<T>(std::move(storage_));
    }

    [[nodiscard]] const T& operator*() const& { return value(); }

    [[nodiscard]] T& operator*() & { return value(); }

    [[nodiscard]] const T* operator->() const { return &value(); }

    [[nodiscard]] T* operator->() { return &value(); }

    [[nodiscard]] const E& error() const& {
        assert(!has_value());
        return std::get<E>(storage_);
    }

    [[nodiscard]] E& error() & {
        assert(!has_value());
        return std::get<E>(storage_);
    }

    [[nodiscard]] E&& error() && {
        assert(!has_value());
        return std::get<E>(std::move(storage_));
    }

    template <typename U>
    [[nodiscard]] T value_or(U&& default_value) const& {
        if (has_value()) {
            return value();
        }
        return static_cast<T>(std::forward<U>(default_value));
    }

private:
    std::variant<T, E> storage_;
};

// Specialization for void
template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    Result() : error_{} {}
    Result(const E& err) : error_(err) {}
    Result(E&& err) : error_(std::move(err)) {}

    [[nodiscard]] bool has_value() const noexcept { return !error_.has_value(); }

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] const E& error() const& {
        assert(!has_value());
        return *error_;
    }

    [[nodiscard]] E& error() & {
        assert(!has_value());
        return *error_;
    }

    [[nodiscard]] E&& error() && {
        assert(!has_value());
        return *std::move(error_);
    }

    static Result<void, E> Ok() noexcept { return Result<void, E>(); }

private:
    std::optional<E> error_{std::nullopt};
};

} // namespace xblob
