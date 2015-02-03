/****************************************************************************
 *   Copyright (C) 2014-2015 by Savoir-Faire Linux                          *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> *
 *                                                                          *
 *   This library is free software you can redistribute it and/or           *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation either            *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
#include "certificate.h"

//Qt
#include <QtCore/QFile>

//Ring daemon
#include <security_const.h>

//Ring
#include "dbus/configurationmanager.h"
#include <certificatemodel.h>

class DetailsCache {
public:
   DetailsCache(const MapStringString& details);

   QDateTime  m_ExpirationDate            ;
   QDateTime  m_ActivationDate            ;
   bool       m_RequirePrivateKeyPassword ;
   QByteArray m_PublicSignature           ;
   int        m_VersionNumber             ;
   QByteArray m_SerialNumber              ;
   QString    m_Issuer                    ;
   QByteArray m_SubjectKeyAlgorithm       ;
   QString    m_Cn                        ;
   QString    m_N                         ;
   QString    m_O                         ;
   QByteArray m_SignatureAlgorithm        ;
   QByteArray m_Md5Fingerprint            ;
   QByteArray m_Sha1Fingerprint           ;
   QByteArray m_PublicKeyId               ;
   QByteArray m_IssuerDn                  ;
   QDateTime  m_NextExpectedUpdateDate    ;
   QString    m_OutgoingServer            ;

};

class ChecksCache {
public:
   ChecksCache(const MapStringString& checks);

   Certificate::CheckValues m_HasPrivateKey                       ;
   Certificate::CheckValues m_IsExpired                           ;
   Certificate::CheckValues m_HasStrongSigning                    ;
   Certificate::CheckValues m_IsSelfSigned                        ;
   Certificate::CheckValues m_PrivateKeyMatch                     ;
   Certificate::CheckValues m_ArePrivateKeyStoragePermissionOk    ;
   Certificate::CheckValues m_ArePublicKeyStoragePermissionOk     ;
   Certificate::CheckValues m_ArePrivateKeyDirectoryPermissionsOk ;
   Certificate::CheckValues m_ArePublicKeyDirectoryPermissionsOk  ;
   Certificate::CheckValues m_ArePrivateKeyStorageLocationOk      ;
   Certificate::CheckValues m_ArePublicKeyStorageLocationOk       ;
   Certificate::CheckValues m_ArePrivateKeySelinuxAttributesOk    ;
   Certificate::CheckValues m_ArePublicKeySelinuxAttributesOk     ;
   Certificate::CheckValues m_Exist                               ;
   Certificate::CheckValues m_IsValid                             ;
   Certificate::CheckValues m_ValidAuthority                      ;
   Certificate::CheckValues m_HasKnownAuthority                   ;
   Certificate::CheckValues m_IsNotRevoked                        ;
   Certificate::CheckValues m_AuthorityMismatch                   ;
   Certificate::CheckValues m_UnexpectedOwner                     ;
   Certificate::CheckValues m_NotActivated                        ;
};

class CertificatePrivate
{
public:
   //Types
   typedef Certificate::CheckValues (Certificate::*accessor)();

   //Attributes
   CertificatePrivate();
   ~CertificatePrivate();
   QUrl m_Path;
   Certificate::Type m_Type;

   mutable DetailsCache* m_pDetailsCache;
   mutable ChecksCache*  m_pCheckCache  ;

   //Helpers
   void loadDetails();
   void loadChecks ();

   static Matrix1D<Certificate::Checks ,QString> m_slChecksName;
   static Matrix1D<Certificate::Checks ,QString> m_slChecksDescription;
   static Matrix1D<Certificate::Details,QString> m_slDetailssName;
   static Matrix1D<Certificate::Details,QString> m_slDetailssDescription;

   static Certificate::CheckValues toBool(const QString& string);
};

Matrix1D<Certificate::Checks ,QString> CertificatePrivate::m_slChecksName = {{
   /* HAS_PRIVATE_KEY                   */ QObject::tr("Has a private key"                               ),
   /* EXPIRED                           */ QObject::tr("Is not expired"                                  ),
   /* STRONG_SIGNING                    */ QObject::tr("Has strong signing"                              ),
   /* NOT_SELF_SIGNED                   */ QObject::tr("Is not self signed"                              ),
   /* KEY_MATCH                         */ QObject::tr("Have a matching key pair"                        ),
   /* PRIVATE_KEY_STORAGE_PERMISSION    */ QObject::tr("Has the right private key file permissions"      ),
   /* PUBLIC_KEY_STORAGE_PERMISSION     */ QObject::tr("Has the right public key file permissions"       ),
   /* PRIVATE_KEY_DIRECTORY_PERMISSIONS */ QObject::tr("Has the right private key directory permissions" ),
   /* PUBLIC_KEY_STORAGE_PERMISSION     */ QObject::tr("Has the right public key directory permissions"  ),
   /* PRIVATE_KEY_STORAGE_LOCATION      */ QObject::tr("Has the right private key directory location"    ),
   /* PUBLIC_KEY_STORAGE_LOCATION       */ QObject::tr("Has the right public key directory location"     ),
   /* PRIVATE_KEY_SELINUX_ATTRIBUTES    */ QObject::tr("Has the right private key SELinux attributes"    ),
   /* PUBLIC_KEY_SELINUX_ATTRIBUTES     */ QObject::tr("Has the right public key SELinux attributes"     ),
   /* EXIST                             */ QObject::tr("The certificate file exist and is readable"      ),
   /* VALID                             */ QObject::tr("The file is a valid certificate"                 ),
   /* VALID_AUTHORITY                   */ QObject::tr("The certificate has a valid authority"           ),
   /* KNOWN_AUTHORITY                   */ QObject::tr("The certificate has a known authority"           ),
   /* NOT_REVOKED                       */ QObject::tr("The certificate is not revoked"                  ),
   /* AUTHORITY_MATCH                   */ QObject::tr("The certificate authority match"                 ),
   /* EXPECTED_OWNER                    */ QObject::tr("The certificate has the expected owner"          ),
   /* ACTIVATED                         */ QObject::tr("The certificate is within its active period"     ),
}};

Matrix1D<Certificate::Checks ,QString> CertificatePrivate::m_slChecksDescription = {{
   /* HAS_PRIVATE_KEY                   */ QObject::tr("TODO"),
   /* EXPIRED                           */ QObject::tr("TODO"),
   /* STRONG_SIGNING                    */ QObject::tr("TODO"),
   /* NOT_SELF_SIGNED                   */ QObject::tr("TODO"),
   /* KEY_MATCH                         */ QObject::tr("TODO"),
   /* PRIVATE_KEY_STORAGE_PERMISSION    */ QObject::tr("TODO"),
   /* PUBLIC_KEY_STORAGE_PERMISSION     */ QObject::tr("TODO"),
   /* PRIVATE_KEY_DIRECTORY_PERMISSIONS */ QObject::tr("TODO"),
   /* PUBLIC_KEY_DIRECTORY_PERMISSIONS  */ QObject::tr("TODO"),
   /* PRIVATE_KEY_STORAGE_LOCATION      */ QObject::tr("TODO"),
   /* PUBLIC_KEY_STORAGE_LOCATION       */ QObject::tr("TODO"),
   /* PRIVATE_KEY_SELINUX_ATTRIBUTES    */ QObject::tr("TODO"),
   /* PUBLIC_KEY_SELINUX_ATTRIBUTES     */ QObject::tr("TODO"),
   /* EXIST                             */ QObject::tr("TODO"),
   /* VALID                             */ QObject::tr("TODO"),
   /* VALID_AUTHORITY                   */ QObject::tr("TODO"),
   /* KNOWN_AUTHORITY                   */ QObject::tr("TODO"),
   /* NOT_REVOKED                       */ QObject::tr("TODO"),
   /* AUTHORITY_MATCH                   */ QObject::tr("TODO"),
   /* EXPECTED_OWNER                    */ QObject::tr("TODO"),
   /* ACTIVATED                         */ QObject::tr("TODO"),
}};

Matrix1D<Certificate::Details,QString> CertificatePrivate::m_slDetailssName = {{
   /* EXPIRATION_DATE                */ QObject::tr("Expiration date"               ),
   /* ACTIVATION_DATE                */ QObject::tr("Activation date"               ),
   /* REQUIRE_PRIVATE_KEY_PASSWORD   */ QObject::tr("Require a private key password"),
   /* PUBLIC_SIGNATURE               */ QObject::tr("Public signature"              ),
   /* VERSION_NUMBER                 */ QObject::tr("Version"                       ),
   /* SERIAL_NUMBER                  */ QObject::tr("Serial number"                 ),
   /* ISSUER                         */ QObject::tr("Issuer"                        ),
   /* SUBJECT_KEY_ALGORITHM          */ QObject::tr("Subject key algorithm"         ),
   /* CN                             */ QObject::tr("Common name (CN)"              ),
   /* N                              */ QObject::tr("Name (N)"                      ),
   /* O                              */ QObject::tr("Organization (O)"              ),
   /* SIGNATURE_ALGORITHM            */ QObject::tr("Signature algorithm"           ),
   /* MD5_FINGERPRINT                */ QObject::tr("Md5 fingerprint"               ),
   /* SHA1_FINGERPRINT               */ QObject::tr("Sha1 fingerprint"              ),
   /* PUBLIC_KEY_ID                  */ QObject::tr("Public key id"                 ),
   /* ISSUER_DN                      */ QObject::tr("Issuer domain name"            ),
   /* NEXT_EXPECTED_UPDATE_DATE      */ QObject::tr("Next expected update"          ),
   /* OUTGOING_SERVER                */ QObject::tr("Outgoing server"               ),

}};

Matrix1D<Certificate::Details,QString> CertificatePrivate::m_slDetailssDescription = {{
   /* EXPIRATION_DATE                 */ QObject::tr("TODO"),
   /* ACTIVATION_DATE                 */ QObject::tr("TODO"),
   /* REQUIRE_PRIVATE_KEY_PASSWORD    */ QObject::tr("TODO"),
   /* PUBLIC_SIGNATURE                */ QObject::tr("TODO"),
   /* VERSION_NUMBER                  */ QObject::tr("TODO"),
   /* SERIAL_NUMBER                   */ QObject::tr("TODO"),
   /* ISSUER                          */ QObject::tr("TODO"),
   /* SUBJECT_KEY_ALGORITHM           */ QObject::tr("TODO"),
   /* CN                              */ QObject::tr("TODO"),
   /* N                               */ QObject::tr("TODO"),
   /* O                               */ QObject::tr("TODO"),
   /* SIGNATURE_ALGORITHM             */ QObject::tr("TODO"),
   /* MD5_FINGERPRINT                 */ QObject::tr("TODO"),
   /* SHA1_FINGERPRINT                */ QObject::tr("TODO"),
   /* PUBLIC_KEY_ID                   */ QObject::tr("TODO"),
   /* ISSUER_DN                       */ QObject::tr("TODO"),
   /* NEXT_EXPECTED_UPDATE_DATE       */ QObject::tr("TODO"),
   /* OUTGOING_SERVER                 */ QObject::tr("TODO"),

}};


CertificatePrivate::CertificatePrivate() :
m_pCheckCache(nullptr), m_pDetailsCache(nullptr)
{
}

CertificatePrivate::~CertificatePrivate()
{
   if (m_pDetailsCache) delete m_pDetailsCache;
   if (m_pCheckCache  ) delete m_pCheckCache  ;
}

Certificate::CheckValues CertificatePrivate::toBool(const QString& string)
{
   if (string == DRing::Certificate::CheckValuesNames::PASSED)
      return Certificate::CheckValues::PASSED;
   else if (string == DRing::Certificate::CheckValuesNames::FAILED)
      return Certificate::CheckValues::FAILED;
   else
      return Certificate::CheckValues::UNSUPPORTED;
}

DetailsCache::DetailsCache(const MapStringString& details)
{
   m_ExpirationDate             = QDateTime::fromString( details[DRing::Certificate::DetailsNames::EXPIRATION_DATE ],"yyyy-mm-dd");
   m_ActivationDate             = QDateTime::fromString( details[DRing::Certificate::DetailsNames::ACTIVATION_DATE ],"yyyy-mm-dd");
   m_RequirePrivateKeyPassword  = false;//TODO//details[DRing::Certificate::DetailsNames::REQUIRE_PRIVATE_KEY_PASSWORD].toBool();
   m_PublicSignature            = details[DRing::Certificate::DetailsNames::PUBLIC_SIGNATURE            ].toAscii();
   m_VersionNumber              = details[DRing::Certificate::DetailsNames::VERSION_NUMBER              ].toInt();
   m_SerialNumber               = details[DRing::Certificate::DetailsNames::SERIAL_NUMBER               ].toAscii();
   m_Issuer                     = details[DRing::Certificate::DetailsNames::ISSUER                      ];
   m_SubjectKeyAlgorithm        = details[DRing::Certificate::DetailsNames::SUBJECT_KEY_ALGORITHM       ].toAscii();
   m_Cn                         = details[DRing::Certificate::DetailsNames::CN                          ];
   m_N                          = details[DRing::Certificate::DetailsNames::N                           ];
   m_O                          = details[DRing::Certificate::DetailsNames::O                           ];
   m_SignatureAlgorithm         = details[DRing::Certificate::DetailsNames::SIGNATURE_ALGORITHM         ].toAscii();
   m_Md5Fingerprint             = details[DRing::Certificate::DetailsNames::MD5_FINGERPRINT             ].toAscii();
   m_Sha1Fingerprint            = details[DRing::Certificate::DetailsNames::SHA1_FINGERPRINT            ].toAscii();
   m_PublicKeyId                = details[DRing::Certificate::DetailsNames::PUBLIC_KEY_ID               ].toAscii();
   m_IssuerDn                   = details[DRing::Certificate::DetailsNames::ISSUER_DN                   ].toAscii();
   m_NextExpectedUpdateDate     = QDateTime::fromString(details[DRing::Certificate::DetailsNames::NEXT_EXPECTED_UPDATE_DATE ],"yyyy-mm-dd");
   m_OutgoingServer             = details[DRing::Certificate::DetailsNames::OUTGOING_SERVER             ] ;
}

ChecksCache::ChecksCache(const MapStringString& checks)
{
   m_HasPrivateKey                       = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::HAS_PRIVATE_KEY                  ]);
   m_IsExpired                           = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::EXPIRED                          ]);
   m_HasStrongSigning                    = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::STRONG_SIGNING                   ]);
   m_IsSelfSigned                        = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::NOT_SELF_SIGNED                  ]);
   m_PrivateKeyMatch                     = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::KEY_MATCH                        ]);
   m_ArePrivateKeyStoragePermissionOk    = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_PERMISSION   ]);
   m_ArePublicKeyStoragePermissionOk     = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_PERMISSION    ]);
   m_ArePrivateKeyDirectoryPermissionsOk = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PRIVATE_KEY_DIRECTORY_PERMISSIONS]);
   m_ArePublicKeyDirectoryPermissionsOk  = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PUBLIC_KEY_DIRECTORY_PERMISSIONS ]);
   m_ArePrivateKeyStorageLocationOk      = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PRIVATE_KEY_STORAGE_LOCATION     ]);
   m_ArePublicKeyStorageLocationOk       = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PUBLIC_KEY_STORAGE_LOCATION      ]);
   m_ArePrivateKeySelinuxAttributesOk    = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PRIVATE_KEY_SELINUX_ATTRIBUTES   ]);
   m_ArePublicKeySelinuxAttributesOk     = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::PUBLIC_KEY_SELINUX_ATTRIBUTES    ]);
   m_Exist                               = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::EXIST                            ]);
   m_IsValid                             = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::VALID                            ]);
   m_ValidAuthority                      = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::VALID_AUTHORITY                  ]);
   m_HasKnownAuthority                   = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::KNOWN_AUTHORITY                  ]);
   m_IsNotRevoked                        = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::NOT_REVOKED                      ]);
   m_AuthorityMismatch                   = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::AUTHORITY_MISMATCH               ]);
   m_UnexpectedOwner                     = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::UNEXPECTED_OWNER                 ]);
   m_NotActivated                        = CertificatePrivate::toBool(checks[DRing::Certificate::ChecksNames::NOT_ACTIVATED                    ]);
}

void CertificatePrivate::loadDetails()
{
   if (!m_pDetailsCache) {
      const MapStringString d = DBus::ConfigurationManager::instance().getCertificateDetails(m_Path.toString());
      m_pDetailsCache = new DetailsCache(d);
   }
}

void CertificatePrivate::loadChecks()
{
   if (!m_pCheckCache) {
      const MapStringString checks = DBus::ConfigurationManager::instance().validateCertificate(QString(),m_Path.toString(),QString());
      m_pCheckCache = new ChecksCache(checks);
   }
}

Certificate::Certificate(const QUrl& path, Type type, const QUrl& privateKey) : QObject(CertificateModel::instance()),d_ptr(new CertificatePrivate())
{
   Q_UNUSED(privateKey)
   d_ptr->m_Path = path.path();
   d_ptr->m_Type = type;
}

Certificate::~Certificate()
{

}

QString Certificate::getName(Certificate::Checks check)
{
   return CertificatePrivate::m_slChecksName[check];
}

QString Certificate::getName(Certificate::Details detail)
{
   return CertificatePrivate::m_slDetailssName[detail];
}

QString Certificate::getDescription(Certificate::Checks check)
{
   return CertificatePrivate::m_slChecksDescription[check];
}

QString Certificate::getDescription(Certificate::Details detail)
{
   return CertificatePrivate::m_slDetailssDescription[detail];
}

Certificate::CheckValues Certificate::hasPrivateKey() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_HasPrivateKey;
}

Certificate::CheckValues Certificate::isNotExpired() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_IsExpired;
}

Certificate::CheckValues Certificate::hasStrongSigning() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_HasStrongSigning;
}

Certificate::CheckValues Certificate::isNotSelfSigned() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_IsSelfSigned;
}

Certificate::CheckValues Certificate::privateKeyMatch() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_PrivateKeyMatch;
}

Certificate::CheckValues Certificate::arePrivateKeyStoragePermissionOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePrivateKeyStoragePermissionOk;
}

Certificate::CheckValues Certificate::arePublicKeyStoragePermissionOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePublicKeyStoragePermissionOk;
}

Certificate::CheckValues Certificate::arePrivateKeyDirectoryPermissionsOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePrivateKeyDirectoryPermissionsOk;
}

Certificate::CheckValues Certificate::arePublicKeyDirectoryPermissionsOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePublicKeyDirectoryPermissionsOk;
}

Certificate::CheckValues Certificate::arePrivateKeyStorageLocationOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePrivateKeyStorageLocationOk;
}

Certificate::CheckValues Certificate::arePublicKeyStorageLocationOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePublicKeyStorageLocationOk;
}

Certificate::CheckValues Certificate::arePrivateKeySelinuxAttributesOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePrivateKeySelinuxAttributesOk;
}

Certificate::CheckValues Certificate::arePublicKeySelinuxAttributesOk() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ArePublicKeySelinuxAttributesOk;
}

Certificate::CheckValues Certificate::exist() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_Exist;
}

Certificate::CheckValues Certificate::isValid() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_IsValid;
}

Certificate::CheckValues Certificate::hasValidAuthority() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_ValidAuthority;
}

Certificate::CheckValues Certificate::hasKnownAuthority() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_HasKnownAuthority;
}

Certificate::CheckValues Certificate::isNotRevoked() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_IsNotRevoked;
}

Certificate::CheckValues Certificate::authorityMatch() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_AuthorityMismatch;
}

Certificate::CheckValues Certificate::hasExpectedOwner() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pCheckCache->m_UnexpectedOwner;
}

QDateTime Certificate::expirationDate() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_ExpirationDate;
}

QDateTime Certificate::activationDate() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_ActivationDate;
}

bool Certificate::requirePrivateKeyPassword() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_RequirePrivateKeyPassword;
}

QByteArray Certificate::publicSignature() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_PublicSignature;
}

int Certificate::versionNumber() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_VersionNumber;
}

QByteArray Certificate::serialNumber() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_SerialNumber;
}

QString Certificate::issuer() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_Issuer;
}

QByteArray Certificate::subjectKeyAlgorithm() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_SubjectKeyAlgorithm;
}

QString Certificate::cn() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_Cn;
}

QString Certificate::n() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_N;
}

QString Certificate::o() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_O;
}

QByteArray Certificate::signatureAlgorithm() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_SignatureAlgorithm;
}

QByteArray Certificate::md5Fingerprint() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_Md5Fingerprint;
}

QByteArray Certificate::sha1Fingerprint() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_Sha1Fingerprint;
}

QByteArray Certificate::publicKeyId() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_PublicKeyId;
}

QByteArray Certificate::issuerDn() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_IssuerDn;
}

QDateTime Certificate::nextExpectedUpdateDate() const
{
   d_ptr->loadDetails();
   return d_ptr->m_pDetailsCache->m_NextExpectedUpdateDate;
}

bool Certificate::isActivated() const {
   d_ptr->loadDetails();
   return d_ptr->m_pCheckCache->m_NotActivated == Certificate::CheckValues::PASSED;
}


void Certificate::setPath(const QUrl& path)
{
   d_ptr->m_Path = path;
}

QUrl Certificate::path() const
{
   return d_ptr->m_Path;
}

Certificate::Type Certificate::type() const
{
   return d_ptr->m_Type;
}

QString Certificate::outgoingServer() const
{
   d_ptr->loadChecks();
   return d_ptr->m_pDetailsCache->m_OutgoingServer;
}


Certificate::CheckValues Certificate::checkResult(Certificate::Checks check) const
{
   switch (check) {
      case Checks::HAS_PRIVATE_KEY                   : return hasPrivateKey                       ();
      case Checks::EXPIRED                           : return isNotExpired                        ();
      case Checks::STRONG_SIGNING                    : return hasStrongSigning                    ();
      case Checks::NOT_SELF_SIGNED                   : return isNotSelfSigned                     ();
      case Checks::KEY_MATCH                         : return privateKeyMatch                     ();
      case Checks::PRIVATE_KEY_STORAGE_PERMISSION    : return arePrivateKeyStoragePermissionOk    ();
      case Checks::PUBLIC_KEY_STORAGE_PERMISSION     : return arePublicKeyStoragePermissionOk     ();
      case Checks::PRIVATE_KEY_DIRECTORY_PERMISSIONS : return arePrivateKeyDirectoryPermissionsOk ();
      case Checks::PUBLIC_KEY_DIRECTORY_PERMISSIONS  : return arePublicKeyDirectoryPermissionsOk  ();
      case Checks::PRIVATE_KEY_STORAGE_LOCATION      : return arePrivateKeyStorageLocationOk      ();
      case Checks::PUBLIC_KEY_STORAGE_LOCATION       : return arePublicKeyStorageLocationOk       ();
      case Checks::PRIVATE_KEY_SELINUX_ATTRIBUTES    : return arePrivateKeySelinuxAttributesOk    ();
      case Checks::PUBLIC_KEY_SELINUX_ATTRIBUTES     : return arePublicKeySelinuxAttributesOk     ();
      case Checks::EXIST                             : return exist                               ();
      case Checks::VALID                             : return isValid                             ();
      case Checks::VALID_AUTHORITY                   : return hasValidAuthority                   ();
      case Checks::KNOWN_AUTHORITY                   : return hasKnownAuthority                   ();
      case Checks::NOT_REVOKED                       : return isNotRevoked                        ();
      case Checks::AUTHORITY_MATCH                   : return authorityMatch                      ();
      case Checks::EXPECTED_OWNER                    : return hasExpectedOwner                    ();
      case Checks::ACTIVATED                         : return static_cast<Certificate::CheckValues>(isActivated());
      case Checks::COUNT__:
         Q_ASSERT(false);
   };
   return Certificate::CheckValues::UNSUPPORTED;
}

QVariant Certificate::detailResult(Certificate::Details detail) const
{
   switch(detail) {
      case Details::EXPIRATION_DATE                : return expirationDate                ();
      case Details::ACTIVATION_DATE                : return activationDate                ();
      case Details::REQUIRE_PRIVATE_KEY_PASSWORD   : return requirePrivateKeyPassword     ();
      case Details::PUBLIC_SIGNATURE               : return publicSignature               ();
      case Details::VERSION_NUMBER                 : return versionNumber                 ();
      case Details::SERIAL_NUMBER                  : return serialNumber                  ();
      case Details::ISSUER                         : return issuer                        ();
      case Details::SUBJECT_KEY_ALGORITHM          : return subjectKeyAlgorithm           ();
      case Details::CN                             : return cn                            ();
      case Details::N                              : return n                             ();
      case Details::O                              : return o                             ();
      case Details::SIGNATURE_ALGORITHM            : return signatureAlgorithm            ();
      case Details::MD5_FINGERPRINT                : return md5Fingerprint                ();
      case Details::SHA1_FINGERPRINT               : return sha1Fingerprint               ();
      case Details::PUBLIC_KEY_ID                  : return publicKeyId                   ();
      case Details::ISSUER_DN                      : return issuerDn                      ();
      case Details::NEXT_EXPECTED_UPDATE_DATE      : return nextExpectedUpdateDate        ();
      case Details::OUTGOING_SERVER                : return outgoingServer                ();

      case Details::COUNT__:
         Q_ASSERT(false);
   };
   return QVariant();
}
