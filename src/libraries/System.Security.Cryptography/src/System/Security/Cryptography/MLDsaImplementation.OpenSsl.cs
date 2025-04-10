// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;

namespace System.Security.Cryptography
{
    internal sealed partial class MLDsaImplementation : MLDsa
    {
        private SafeEvpPKeyHandle _key = null!;

        private MLDsaImplementation(MLDsaAlgorithm algorithm, SafeEvpPKeyHandle key)
            : base(algorithm)
        {
            Debug.Assert(key is not null);
            Debug.Assert(!key.IsInvalid);
            Debug.Assert(SupportsAny());

            _key = key;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                _key?.Dispose();
                _key = null!;
            }

            base.Dispose(disposing);
        }

        internal static partial bool SupportsAny() =>
            Interop.Crypto.EvpPKeyMLDsaAlgs.MLDsa44 != null ||
            Interop.Crypto.EvpPKeyMLDsaAlgs.MLDsa65 != null ||
            Interop.Crypto.EvpPKeyMLDsaAlgs.MLDsa87 != null;

        internal MLDsaImplementation Duplicate()
        {
            return new MLDsaImplementation(Algorithm, _key.DuplicateHandle());
        }

        internal SafeEvpPKeyHandle DuplicateHandle()
        {
            return _key.DuplicateHandle();
        }

        // TODO: Delete this when public MLDsaOpenSsl is added.
        internal static MLDsaImplementation FromHandle(MLDsaAlgorithm algorithm, SafeEvpPKeyHandle key)
        {
            Debug.Assert(key is not null);
            Debug.Assert(!key.IsInvalid);
            Debug.Assert(algorithm is not null);

            ThrowIfNotSupported();
            return new MLDsaImplementation(algorithm, key.DuplicateHandle());
        }

        protected override void SignDataCore(ReadOnlySpan<byte> data, ReadOnlySpan<byte> context, Span<byte> destination) =>
            Interop.Crypto.MLDsaSignPure(_key, data, context, destination);

        protected override bool VerifyDataCore(ReadOnlySpan<byte> data, ReadOnlySpan<byte> context, ReadOnlySpan<byte> signature) =>
            Interop.Crypto.MLDsaVerifyPure(_key, data, context, signature);

        protected override void ExportMLDsaPublicKeyCore(Span<byte> destination) =>
            Interop.Crypto.MLDsaExportPublicKey(_key, destination);

        protected override void ExportMLDsaSecretKeyCore(Span<byte> destination) =>
            Interop.Crypto.MLDsaExportSecretKey(_key, destination);

        protected override void ExportMLDsaPrivateSeedCore(Span<byte> destination) =>
            Interop.Crypto.MLDsaExportSeed(_key, destination);

        internal static partial MLDsaImplementation GenerateKeyImpl(MLDsaAlgorithm algorithm)
        {
            SafeEvpPKeyHandle key = Interop.Crypto.MLDsaGenerateKey(algorithm.Name, ReadOnlySpan<byte>.Empty);
            return new MLDsaImplementation(algorithm, key);
        }

        internal static partial MLDsaImplementation ImportPublicKey(MLDsaAlgorithm algorithm, ReadOnlySpan<byte> source)
        {
            Debug.Assert(source.Length == algorithm.PublicKeySizeInBytes, $"Public key was expected to be {algorithm.PublicKeySizeInBytes} bytes, but was {source.Length} bytes.");
            SafeEvpPKeyHandle key = Interop.Crypto.EvpPKeyFromData(algorithm.Name, source, privateKey: false);
            return new MLDsaImplementation(algorithm, key);
        }

        internal static partial MLDsaImplementation ImportPkcs8PrivateKeyValue(MLDsaAlgorithm algorithm, ReadOnlySpan<byte> source) =>
            throw new PlatformNotSupportedException();

        internal static partial MLDsaImplementation ImportSecretKey(MLDsaAlgorithm algorithm, ReadOnlySpan<byte> source)
        {
            Debug.Assert(source.Length == algorithm.SecretKeySizeInBytes, $"Secret key was expected to be {algorithm.SecretKeySizeInBytes} bytes, but was {source.Length} bytes.");
            SafeEvpPKeyHandle key = Interop.Crypto.EvpPKeyFromData(algorithm.Name, source, privateKey: true);
            return new MLDsaImplementation(algorithm, key);
        }

        internal static partial MLDsaImplementation ImportSeed(MLDsaAlgorithm algorithm, ReadOnlySpan<byte> source)
        {
            Debug.Assert(source.Length == algorithm.PrivateSeedSizeInBytes, $"Seed was expected to be {algorithm.PrivateSeedSizeInBytes} bytes, but was {source.Length} bytes.");
            SafeEvpPKeyHandle key = Interop.Crypto.MLDsaGenerateKey(algorithm.Name, source);
            return new MLDsaImplementation(algorithm, key);
        }
    }
}
