// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Diagnostics.CodeAnalysis;
using System.Runtime.Versioning;

namespace System.Security.Cryptography
{
    /// <summary>
    ///   Represents an ML-KEM key backed by OpenSSL.
    /// </summary>
    /// <remarks>
    ///   <para>
    ///     This algorithm is specified by FIPS-203.
    ///   </para>
    ///   <para>
    ///     Developers are encouraged to program against the <c>MLKem</c> base class,
    ///     rather than any specific derived class.
    ///     The derived classes are intended for interop with the underlying system
    ///     cryptographic libraries.
    ///   </para>
    /// </remarks>
    [Experimental(Experimentals.PostQuantumCryptographyDiagId)]
    public sealed partial class MLKemOpenSsl : MLKem
    {
        /// <summary>
        ///   Initializes a new instance of the <see cref="MLKemOpenSsl" /> class from an existing OpenSSL key
        ///   represented as an <c>EVP_PKEY*</c>.
        /// </summary>
        /// <param name="pkeyHandle">
        ///   The OpenSSL <c>EVP_PKEY*</c> value to use as the key, represented as a <see cref="SafeEvpPKeyHandle" />.
        /// </param>
        /// <exception cref="ArgumentNullException">
        ///   <paramref name="pkeyHandle" /> is <see langword="null" />.
        /// </exception>
        /// <exception cref="CryptographicException">
        ///   <para>The handle in <paramref name="pkeyHandle" /> is not recognized as an ML-KEM key.</para>
        ///   <para>-or-</para>
        ///   <para>An error occurred while creating the algorithm instance.</para>
        /// </exception>
        /// <exception cref="ObjectDisposedException">
        ///   The handle in <paramref name="pkeyHandle" /> is already disposed.
        /// </exception>
        [UnsupportedOSPlatform("android")]
        [UnsupportedOSPlatform("browser")]
        [UnsupportedOSPlatform("ios")]
        [UnsupportedOSPlatform("osx")]
        [UnsupportedOSPlatform("tvos")]
        [UnsupportedOSPlatform("windows")]
        public MLKemOpenSsl(SafeEvpPKeyHandle pkeyHandle) : base(AlgorithmFromHandle(pkeyHandle, out SafeEvpPKeyHandle upRefHandle))
        {
            Initialize(upRefHandle);
        }

        // This partial can go away if partial constructors are available.
        // https://github.com/dotnet/csharplang/issues/9058
        private partial void Initialize(SafeEvpPKeyHandle upRefHandle);
        private static partial MLKemAlgorithm AlgorithmFromHandle(SafeEvpPKeyHandle pkeyHandle, out SafeEvpPKeyHandle upRefHandle);

        /// <summary>
        /// Creates a duplicate handle.
        /// </summary>
        /// <returns>A SafeHandle for the ML-KEM key in OpenSSL.</returns>
        /// <exception cref="ObjectDisposedException">The object has already been disposed.</exception>
        public partial SafeEvpPKeyHandle DuplicateKeyHandle();
    }
}
