// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Security.Authentication;
using System.Security.Cryptography.X509Certificates;

namespace System.Net.Security
{
    public class SslServerAuthenticationOptions
    {
        private X509RevocationMode _checkCertificateRevocation = X509RevocationMode.NoCheck;
        private SslProtocols _enabledSslProtocols = SslProtocols.None;
        private EncryptionPolicy _encryptionPolicy = EncryptionPolicy.RequireEncryption;
        private bool _allowRenegotiation;
        private bool _allowTlsResume = true;

        public bool AllowRenegotiation
        {
            get => _allowRenegotiation;
            set => _allowRenegotiation = value;
        }

        /// <summary>
        ///  Gets or sets a value that indicates whether the SslStream should allow TLS resumption.
        /// </summary>
        public bool AllowTlsResume
        {
            get => _allowTlsResume;
            set => _allowTlsResume = value;
        }

        public bool ClientCertificateRequired { get; set; }

        public List<SslApplicationProtocol>? ApplicationProtocols { get; set; }

        public RemoteCertificateValidationCallback? RemoteCertificateValidationCallback { get; set; }

        public ServerCertificateSelectionCallback? ServerCertificateSelectionCallback { get; set; }

        public X509Certificate? ServerCertificate { get; set; }

        public SslStreamCertificateContext? ServerCertificateContext { get; set; }

        public SslProtocols EnabledSslProtocols
        {
            get => _enabledSslProtocols;
            set => _enabledSslProtocols = value;
        }

        public X509RevocationMode CertificateRevocationCheckMode
        {
            get => _checkCertificateRevocation;
            set
            {
                if (value != X509RevocationMode.NoCheck && value != X509RevocationMode.Offline && value != X509RevocationMode.Online)
                {
                    throw new ArgumentException(SR.Format(SR.net_invalid_enum, nameof(X509RevocationMode)), nameof(value));
                }

                _checkCertificateRevocation = value;
            }
        }

        public EncryptionPolicy EncryptionPolicy
        {
            get => _encryptionPolicy;
            set
            {
#pragma warning disable SYSLIB0040 // NoEncryption and AllowNoEncryption are obsolete
                if (value != EncryptionPolicy.RequireEncryption && value != EncryptionPolicy.AllowNoEncryption && value != EncryptionPolicy.NoEncryption)
                {
                    throw new ArgumentException(SR.Format(SR.net_invalid_enum, nameof(EncryptionPolicy)), nameof(value));
                }
#pragma warning restore SYSLIB0040

                _encryptionPolicy = value;
            }
        }

        /// <summary>
        /// Specifies cipher suites allowed to be used for TLS.
        /// When set to null operating system default will be used.
        /// Use extreme caution when changing this setting.
        /// </summary>
        public CipherSuitesPolicy? CipherSuitesPolicy { get; set; }

        /// <summary>
        /// Gets or sets an optional customized policy for remote certificate
        /// validation. If not <see langword="null"/>,
        /// <see cref="CertificateRevocationCheckMode"/> and <see cref="SslCertificateTrust"/>
        /// are ignored.
        /// </summary>
        public X509ChainPolicy? CertificateChainPolicy { get; set; }
    }
}
