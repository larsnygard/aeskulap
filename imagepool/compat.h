/*
    Aeskulap 0.2.9 — DCMTK 3.7 compatibility shim.

    Maps the 2005-era dcmnet "CONDITION" API (long typedef, SUCCESS/FAILURE
    macros, COUT/CERR consoles, ERT_* dictionary enums) onto the modern
    OFCondition/ofConsole API without touching call sites.
*/
#ifndef AESKULAP_DCMTK_COMPAT_H
#define AESKULAP_DCMTK_COMPAT_H

#include "dcmtk/config/osconfig.h"
#include "dcmtk/ofstd/ofcond.h"
#include "dcmtk/ofstd/ofconsol.h"
#include "dcmtk/dcmnet/cond.h"
#include "dcmtk/dcmnet/dimse.h"
#include "dcmtk/dcmnet/assoc.h"

// 2005 dcmnet used a long typedef; 3.x uses the OFCondition class.
// Comparisons like (cond == DIMSE_NORMAL) and .good() calls keep working.
typedef OFCondition CONDITION;

// 2005-era console macros. Modern DcmObject::print() takes std::ostream&.
#ifndef COUT
#define COUT  std::cout
#define CERR  std::cerr
#endif

// SUCCESS()/FAILURE() were macros on the long condition codes.
#ifndef SUCCESS
#define SUCCESS(x) ((x).good())
#define FAILURE(x) ((x).bad())
#endif

// Legacy ERT_* record-type enum names still exist in 3.7 (dcdirrec.h).

// 2005 dcmdata symbol renamed in modern DCMTK (word order).
#ifndef numberOfAllDcmStorageSOPClassUIDs
#define numberOfAllDcmStorageSOPClassUIDs numberOfDcmAllStorageSOPClassUIDs
#endif

// Old DICOMDIR condition prefix.
#ifndef ECC_Normal
#define ECC_Normal EC_Normal
#endif

// Modern DCMTK dropped the 'TransferSyntax' suffix from the transfer
// syntax enum constants.
#define EXS_JPEGProcess1TransferSyntax EXS_JPEGProcess1
#define EXS_JPEGProcess2_4TransferSyntax EXS_JPEGProcess2_4
#define EXS_JPEGProcess14SV1TransferSyntax EXS_JPEGProcess14SV1
#define EXS_JPEGProcess14TransferSyntax EXS_JPEGProcess14

// DIMSE_setConsole vanished; ofConsole is the modern equivalent.
#ifndef DIMSE_setConsole
#define DIMSE_setConsole(x) ((void)0)
#endif
#ifndef ASC_setConsole
#define ASC_setConsole(x) ((void)0)
#endif
#ifndef DUL_setConsole
#define DUL_setConsole(x) ((void)0)
#endif

// Old global "normal" conditions map onto EC_Normal; peer conditions
// kept their DUL_ names (cond.h documents the aliasing).
#ifndef ASC_NORMAL
#define ASC_NORMAL                   EC_Normal
#define DIMSE_NORMAL                 EC_Normal
#define DUL_NORMAL                   EC_Normal
#define ASC_ASSOCIATIONREJECTED      DUL_ASSOCIATIONREJECTED
#define DIMSE_PEERABORTEDASSOCIATION DUL_PEERABORTEDASSOCIATION
#define DIMSE_PEERREQUESTEDRELEASE   DUL_PEERREQUESTEDRELEASE
#endif

// 2005 global factory for elements by tag; DCMTK 3.7 has the same factory
// as a static on DcmItem (caller owns the returned element).
#include "dcmtk/dcmdata/dcitem.h"
#ifndef newDicomElement
inline DcmElement* newDicomElement(const DcmTag& tag) {
  // DcmTag derives from DcmTagKey; the static factory takes the key.
  return DcmItem::newDicomElement(static_cast<const DcmTagKey&>(tag));
}
#endif

// Old OFConsole object (capital O) used by 2005 code.
#ifndef OFConsole_legacy
#define OFConsole_legacy ofConsole
#endif

#endif // AESKULAP_DCMTK_COMPAT_H
