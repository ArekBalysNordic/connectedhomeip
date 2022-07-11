/*
 *
 *    Copyright (c) 2022 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "FactoryDataProvider.h"
#include "CHIPDevicePlatformConfig.h"
#include <crypto/CHIPCryptoPAL.h>

#include <logging/log.h>

namespace chip {
namespace {

CHIP_ERROR LoadKeypairFromRaw(ByteSpan privateKey, ByteSpan publicKey, Crypto::P256Keypair & keypair)
{
    Crypto::P256SerializedKeypair serializedKeypair;
    ReturnErrorOnFailure(serializedKeypair.SetLength(privateKey.size() + publicKey.size()));
    memcpy(serializedKeypair.Bytes(), publicKey.data(), publicKey.size());
    memcpy(serializedKeypair.Bytes() + publicKey.size(), privateKey.data(), privateKey.size());
    return keypair.Deserialize(serializedKeypair);
}
} // namespace

namespace DeviceLayer {

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::Init()
{
    uint8_t * factoryData = nullptr;
    size_t factoryDataSize;

    CHIP_ERROR error = mFlashFactoryData.ProtectFactoryDataPartitionAgainstWrite();

    // Protection against write for external storage is not supported.
    if (error == CHIP_ERROR_NOT_IMPLEMENTED)
    {
        ChipLogProgress(DeviceLayer, "The device does not support hardware protection against write.");
        error = CHIP_NO_ERROR;
    }
    else if (error != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to protect the factory data partition.");
        return error;
    }

    error = mFlashFactoryData.GetFactoryDataPartition(factoryData, factoryDataSize);

    if (error != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to read factory data partition");
        return error;
    }

    if (!ParseFactoryData(factoryData, factoryDataSize, &mFactoryData))
    {
        ChipLogError(DeviceLayer, "Failed to parse factory data");
        return CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND;
    }

    // Check if factory data version is correct
    if (mFactoryData.version != CONFIG_CHIP_FACTORY_DATA_VERSION)
    {
        ChipLogError(DeviceLayer, "Factory data version mismatch. Flash version: %d vs code version: %d", mFactoryData.version,
                     CONFIG_CHIP_FACTORY_DATA_VERSION);
        return CHIP_ERROR_VERSION_MISMATCH;
    }

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetCertificationDeclaration(MutableByteSpan & outBuffer)
{
    //-> format_version = 1
    //-> vendor_id = 0xFFF1
    //-> product_id_array = [ 0x8000, 0x8001, 0x8002, 0x8003, 0x8004, 0x8005, 0x8006, 0x8007, 0x8008, 0x8009, 0x800A, 0x800B,
    // 0x800C, 0x800D, 0x800E, 0x800F, 0x8010, 0x8011, 0x8012, 0x8013, 0x8014, 0x8015, 0x8016, 0x8017, 0x8018, 0x8019, 0x801A,
    // 0x801B, 0x801C, 0x801D, 0x801E, 0x801F, 0x8020, 0x8021, 0x8022, 0x8023, 0x8024, 0x8025, 0x8026, 0x8027, 0x8028, 0x8029,
    // 0x802A, 0x802B, 0x802C, 0x802D, 0x802E, 0x802F, 0x8030, 0x8031, 0x8032, 0x8033, 0x8034, 0x8035, 0x8036, 0x8037, 0x8038,
    // 0x8039, 0x803A, 0x803B, 0x803C, 0x803D, 0x803E, 0x803F, 0x8040, 0x8041, 0x8042, 0x8043, 0x8044, 0x8045, 0x8046, 0x8047,
    // 0x8048, 0x8049, 0x804A, 0x804B, 0x804C, 0x804D, 0x804E, 0x804F, 0x8050, 0x8051, 0x8052, 0x8053, 0x8054, 0x8055, 0x8056,
    // 0x8057, 0x8058, 0x8059, 0x805A, 0x805B, 0x805C, 0x805D, 0x805E, 0x805F, 0x8060, 0x8061, 0x8062, 0x8063 ]
    //-> device_type_id = 0x0016
    //-> certificate_id = "ZIG20142ZB330003-24"
    //-> security_level = 0
    //-> security_information = 0
    //-> version_number = 0x2694
    //-> certification_type = 0
    //-> dac_origin_vendor_id is not present
    //-> dac_origin_product_id is not present
    const uint8_t kCdForAllExamples[541] = {
        0x30, 0x81, 0xe8, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0, 0x81, 0xda, 0x30, 0x81, 0xd7,
        0x02, 0x01, 0x03, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30, 0x44,
        0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x37, 0x04, 0x35, 0x15, 0x24, 0x00, 0x01, 0x25,
        0x01, 0x35, 0x47, 0x36, 0x02, 0x05, 0x06, 0x80, 0x18, 0x24, 0x03, 0x0a, 0x2c, 0x04, 0x13, 0x5a, 0x49, 0x47, 0x32, 0x30,
        0x31, 0x34, 0x32, 0x5a, 0x42, 0x33, 0x33, 0x30, 0x30, 0x30, 0x33, 0x2d, 0x32, 0x34, 0x24, 0x05, 0x00, 0x24, 0x06, 0x00,
        0x25, 0x07, 0x94, 0x26, 0x24, 0x08, 0x00, 0x18, 0x31, 0x7d, 0x30, 0x7b, 0x02, 0x01, 0x03, 0x80, 0x14, 0x62, 0xfa, 0x82,
        0x33, 0x59, 0xac, 0xfa, 0xa9, 0x96, 0x3e, 0x1c, 0xfa, 0x14, 0x0a, 0xdd, 0xf5, 0x04, 0xf3, 0x71, 0x60, 0x30, 0x0b, 0x06,
        0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04,
        0x03, 0x02, 0x04, 0x47, 0x30, 0x45, 0x02, 0x20, 0x2f, 0x8f, 0x5c, 0xd9, 0x74, 0xe4, 0x56, 0xe7, 0xc6, 0x8a, 0x68, 0xf6,
        0x57, 0x3d, 0xca, 0x25, 0x52, 0x15, 0x7d, 0x4e, 0x0d, 0x88, 0x62, 0x51, 0x26, 0xaf, 0xd1, 0x98, 0xad, 0x56, 0x7b, 0x2d,
        0x02, 0x21, 0x00, 0xe7, 0x89, 0x5a, 0xf5, 0xed, 0x88, 0xb0, 0x91, 0x7d, 0x4c, 0xc6, 0x17, 0x10, 0x73, 0x51, 0xe0, 0xf5,
        0x62, 0x04, 0xd4, 0x35, 0x6d, 0xa2, 0x88, 0xc6, 0x9a, 0xb6, 0x0a, 0xbf, 0xec, 0x3f, 0x1d
    };

    return CopySpanToMutableSpan(ByteSpan{ kCdForAllExamples }, outBuffer);
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetFirmwareInformation(MutableByteSpan & out_firmware_info_buffer)
{
    out_firmware_info_buffer.reduce_size(0);

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetDeviceAttestationCert(MutableByteSpan & outBuffer)
{
    ReturnErrorCodeIf(outBuffer.size() < mFactoryData.dac_cert.len, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.dac_cert.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(outBuffer.data(), mFactoryData.dac_cert.data, mFactoryData.dac_cert.len);

    outBuffer.reduce_size(mFactoryData.dac_cert.len);

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetProductAttestationIntermediateCert(MutableByteSpan & outBuffer)
{
    ReturnErrorCodeIf(outBuffer.size() < mFactoryData.pai_cert.len, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.pai_cert.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(outBuffer.data(), mFactoryData.pai_cert.data, mFactoryData.pai_cert.len);

    outBuffer.reduce_size(mFactoryData.pai_cert.len);

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::SignWithDeviceAttestationKey(const ByteSpan & messageToSign,
                                                                               MutableByteSpan & outSignBuffer)
{
    Crypto::P256ECDSASignature signature;
    Crypto::P256Keypair keypair;

    VerifyOrReturnError(outSignBuffer.size() >= signature.Capacity(), CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.dac_cert.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    ReturnErrorCodeIf(!mFactoryData.dac_priv_key.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    // Extract public key from DAC cert.
    ByteSpan dacCertSpan{ reinterpret_cast<uint8_t *>(mFactoryData.dac_cert.data), mFactoryData.dac_cert.len };
    chip::Crypto::P256PublicKey dacPublicKey;

    ReturnErrorOnFailure(chip::Crypto::ExtractPubkeyFromX509Cert(dacCertSpan, dacPublicKey));
    ReturnErrorOnFailure(
        LoadKeypairFromRaw(ByteSpan(reinterpret_cast<uint8_t *>(mFactoryData.dac_priv_key.data), mFactoryData.dac_priv_key.len),
                           ByteSpan(dacPublicKey.Bytes(), dacPublicKey.Length()), keypair));
    ReturnErrorOnFailure(keypair.ECDSA_sign_msg(messageToSign.data(), messageToSign.size(), signature));

    return CopySpanToMutableSpan(ByteSpan{ signature.ConstBytes(), signature.Length() }, outSignBuffer);
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSetupDiscriminator(uint16_t & setupDiscriminator)
{
    VerifyOrReturnError(mFactoryData.discriminatorPresent, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    setupDiscriminator = mFactoryData.discriminator;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::SetSetupDiscriminator(uint16_t setupDiscriminator)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSpake2pIterationCount(uint32_t & iterationCount)
{
    ReturnErrorCodeIf(mFactoryData.spake2_it == 0, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    iterationCount = mFactoryData.spake2_it;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSpake2pSalt(MutableByteSpan & saltBuf)
{
    ReturnErrorCodeIf(saltBuf.size() < mFactoryData.spake2_salt.len, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.spake2_salt.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(saltBuf.data(), mFactoryData.spake2_salt.data, mFactoryData.spake2_salt.len);

    saltBuf.reduce_size(mFactoryData.spake2_salt.len);

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSpake2pVerifier(MutableByteSpan & verifierBuf, size_t & verifierLen)
{
    ReturnErrorCodeIf(verifierBuf.size() < mFactoryData.spake2_verifier.len, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.spake2_verifier.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(verifierBuf.data(), mFactoryData.spake2_verifier.data, mFactoryData.spake2_verifier.len);

    verifierLen = mFactoryData.spake2_verifier.len;

    verifierBuf.reduce_size(verifierLen);

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSetupPasscode(uint32_t & setupPasscode)
{
    ReturnErrorCodeIf(mFactoryData.passcode == 0, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    setupPasscode = mFactoryData.passcode;
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::SetSetupPasscode(uint32_t setupPasscode)
{
    return CHIP_ERROR_NOT_IMPLEMENTED;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetVendorName(char * buf, size_t bufSize)
{
    ReturnErrorCodeIf(bufSize < mFactoryData.vendor_name.len + 1, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.vendor_name.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(buf, mFactoryData.vendor_name.data, mFactoryData.vendor_name.len);
    buf[mFactoryData.vendor_name.len] = 0;

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetVendorId(uint16_t & vendorId)
{
    VerifyOrReturnError(mFactoryData.vendorIdPresent, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    vendorId = mFactoryData.vendor_id;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetProductName(char * buf, size_t bufSize)
{
    ReturnErrorCodeIf(bufSize < mFactoryData.product_name.len + 1, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.product_name.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(buf, mFactoryData.product_name.data, mFactoryData.product_name.len);
    buf[mFactoryData.product_name.len] = 0;

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetProductId(uint16_t & productId)
{
    VerifyOrReturnError(mFactoryData.productIdPresent, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    productId = mFactoryData.product_id;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetSerialNumber(char * buf, size_t bufSize)
{
    ReturnErrorCodeIf(bufSize < mFactoryData.sn.len + 1, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.sn.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(buf, mFactoryData.sn.data, mFactoryData.sn.len);
    buf[mFactoryData.sn.len] = 0;

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetManufacturingDate(uint16_t & year, uint8_t & month, uint8_t & day)
{
    VerifyOrReturnError(mFactoryData.date_year != 0, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    year  = mFactoryData.date_year;
    month = mFactoryData.date_month;
    day   = mFactoryData.date_day;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetHardwareVersion(uint16_t & hardwareVersion)
{
    VerifyOrReturnError(mFactoryData.hwVerPresent, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);
    hardwareVersion = mFactoryData.hw_ver;
    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetHardwareVersionString(char * buf, size_t bufSize)
{
    ReturnErrorCodeIf(bufSize < mFactoryData.hw_ver_str.len + 1, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.hw_ver_str.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(buf, mFactoryData.hw_ver_str.data, mFactoryData.hw_ver_str.len);
    buf[mFactoryData.hw_ver_str.len] = 0;

    return CHIP_NO_ERROR;
}

template <class FlashFactoryData>
CHIP_ERROR FactoryDataProvider<FlashFactoryData>::GetRotatingDeviceIdUniqueId(MutableByteSpan & uniqueIdSpan)
{
    ReturnErrorCodeIf(uniqueIdSpan.size() < mFactoryData.rd_uid.len, CHIP_ERROR_BUFFER_TOO_SMALL);
    ReturnErrorCodeIf(!mFactoryData.rd_uid.data, CHIP_ERROR_PERSISTED_STORAGE_VALUE_NOT_FOUND);

    memcpy(uniqueIdSpan.data(), mFactoryData.rd_uid.data, mFactoryData.rd_uid.len);

    return CHIP_NO_ERROR;
}

// Fully instantiate the template class in whatever compilation unit includes this file.
template class FactoryDataProvider<InternalFlashFactoryData>;
template class FactoryDataProvider<ExternalFlashFactoryData>;

} // namespace DeviceLayer
} // namespace chip
