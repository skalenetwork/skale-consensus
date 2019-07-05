//
// Created by kladko on 7/5/19.
//

#include "bls_include.h"

using namespace std;

#include "BLSSigShare.h"
#include "BLSSigShareSet.h"





bool BLSSigShareSet::addSigShare( shared_ptr< BLSSigShare > _sigShare ) {
    if(!_sigShare) {
        BOOST_THROW_EXCEPTION(runtime_error("Null _sigShare"));
    }


    lock_guard< recursive_mutex > lock( sigSharesMutex );

    if ( sigShares.count( _sigShare->getSignerIndex() ) > 0 ) {
        BOOST_THROW_EXCEPTION(runtime_error("Already have this index:" +
                      to_string(_sigShare->getSignerIndex())));
        return false;
    }

    sigShares[_sigShare->getSignerIndex()] = _sigShare;

    return true;
}

size_t BLSSigShareSet::getTotalSigSharesCount() {
    lock_guard< recursive_mutex > lock( sigSharesMutex );
    return sigShares.size();
}
shared_ptr< BLSSigShare > BLSSigShareSet::getSigShareByIndex( size_t _index ) {

    lock_guard< recursive_mutex > lock( sigSharesMutex );

    if ( sigShares.count( _index ) == 0 ) {
        return nullptr;
    }

    return sigShares.at( _index );
}
BLSSigShareSet::BLSSigShareSet(size_t _totalSigners , size_t _requiredSigners)
    : totalSigners(_totalSigners ), requiredSigners(_requiredSigners ) {
    if (_totalSigners == 0) {
        BOOST_THROW_EXCEPTION(runtime_error("_totalSigners == 0"));
    }

    if (totalSigners < _requiredSigners) {
        BOOST_THROW_EXCEPTION(runtime_error("_totalSigners < _requiredSigners"));
    }


}
