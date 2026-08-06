#pragma once

/**
 * @brief Cryptographic validation mode for Bite decryption shares.
 */
enum CryptographicValidationMode : bool {
	Validate = true,
	SkipValidationTrustedSource = false,
};

constexpr bool shouldValidateAsBool(CryptographicValidationMode mode) noexcept {
    return static_cast<bool>(mode);
}