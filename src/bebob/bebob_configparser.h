#ifndef BEBOB_CONFIGPARSER_H
#define BEBOB_CONFIGPARSER_H

#include <vector>

namespace BeBoB {

    typedef struct _VendorModelEntry {
        unsigned int vendor_id;
        unsigned int model_id;
        const char* vendor_name;
        const char* model_name;
    } VendorModelEntry;

    typedef std::vector<VendorModelEntry*> VendorModelEntryVector;

    class ConfigParser {
    public:
        ConfigParser( const char* filename );
        ~ConfigParser();

        const VendorModelEntryVector& getVendorModelEntries() const;
    private:
        VendorModelEntryVector m_vendorModelEntries;
    };

}

#endif
