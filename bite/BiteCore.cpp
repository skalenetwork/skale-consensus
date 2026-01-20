#include "BiteCore.h"
#include "libBLS/threshold_encryption/ThresholdEncryption.h"
#include "libBLS/threshold_encryption/threshold_encryption.h"

std::vector<uint8_t> BiteCore::encryptData( const libBLS::TEPublicKey& _key, 
    const std::vector<uint8_t>& _plainData, const std::optional<std::vector<uint8_t>>& _aadTE ) const {

    if (doRealCrypto) {
        libBLS::EncryptMetaData metaData;
        if (_aadTE) {
            metaData.associatedDataTE = *_aadTE;
        }
        auto cipherText = libBLS::ThresholdEncryption::encrypt(_plainData, _key, metaData);
        return cipherText.toBytes();
    }
    else {
        return libBLS::ThresholdEncryption::mockupEncrypt(_plainData);
    }
}

std::vector<uint8_t> BiteCore::decryptData( const libBLS::AES256Key& _aesKey, 
    std::vector<uint8_t>& _cipherData, bool validate ) const {

    if (doRealCrypto) {
        libBLS::Ciphertext ciphertext = libBLS::Ciphertext::fromBytes(_cipherData, validate);
        return libBLS::ThresholdEncryption::decrypt(ciphertext, _aesKey);
    } else {
        return libBLS::ThresholdEncryption::mockupDecrypt(_cipherData);
    }

}

BiteCore::CiphertextValidationResult BiteCore::validateCiphertexts(
    const std::vector< libBLS::CipheredKey >& ciphertexts,
    const std::vector<std::vector<uint8_t>>* _aadTE) const {

    CiphertextValidationResult result;

    // validate all in parallel, passing AAD if provided
    result.validationResults = libBLS::ThresholdEncryption::validateEncryptionBatchParallel(ciphertexts, _aadTE);

    // true only if all validations passed
    result.allValid = std::all_of(result.validationResults.begin(), result.validationResults.end(),
                                  [](bool v) { return v; });

    // some transactions failed validation getDecryptionSharesFromAESKeys
    if (result.allValid) {
        // convert to string all successful decryption shares
        result.publicDecryptionValues =
            libBLS::CipheredKey::getDecryptionShareInputBatch( ciphertexts );
    }

    return result;
}