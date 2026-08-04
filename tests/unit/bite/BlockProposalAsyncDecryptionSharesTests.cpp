#include "thirdparty/catch.hpp"

#ifdef BITE

#include <future>

#include "BiteTestUtils.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/MyBlockProposal.h"
#include "datastructures/TransactionList.h"
#include "db/TEDecryptionDB.h"
#include "libBLS/test/utils.h"

using namespace std;
using namespace BiteTestUtils;

namespace {

ptr<AESKeyDecryptionShareList> makeEmptyShareList(
    const ptr<BlockProposal> &proposal,
    schain_index decryptorIndex) {
    return make_shared<AESKeyDecryptionShareList>(
        proposal->getBlockID(),
        proposal->getProposerIndex(),
        decryptorIndex);
}

}  // namespace

CATCH_TEST_CASE(
    "BlockProposal waiters resume when decryption shares become ready",
    "[bite][proposal][shares][async][ready]") {
    ConsensusEngine engine(0, 100000000);
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    auto cryptoManager = createTestCryptoManager(chain, node, engine);

    auto kp = generateKeys(1, 1);
    auto proposal = makeAsyncTestProposal(chain, cryptoManager, block_id(501), kp);
    auto readyShares = makeEmptyShareList(proposal, chain->getSchainIndex());

    CATCH_REQUIRE(proposal->tryBeginMyDecryptionSharesComputation());

    promise<void> waiterStarted;
    auto waiterStartedFuture = waiterStarted.get_future();
    auto waiterFinished = std::async(std::launch::async, [&]() {
        waiterStarted.set_value();
        proposal->waitUntilMyDecryptionSharesResolved();
    });

    waiterStartedFuture.wait();
    CATCH_REQUIRE(
        waiterFinished.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout);

    proposal->markMyDecryptionSharesReady(readyShares);

    CATCH_REQUIRE(
        waiterFinished.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CATCH_REQUIRE(proposal->getMyDecryptionShares() == readyShares);
}

CATCH_TEST_CASE(
    "BlockProposal waiters resume when decryption share computation fails",
    "[bite][proposal][shares][async][failed]") {
    ConsensusEngine engine(0, 100000000);
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    auto cryptoManager = createTestCryptoManager(chain, node, engine);

    auto kp = generateKeys(1, 1);
    auto proposal = makeAsyncTestProposal(chain, cryptoManager, block_id(502), kp);

    CATCH_REQUIRE(proposal->tryBeginMyDecryptionSharesComputation());

    promise<void> waiterStarted;
    auto waiterStartedFuture = waiterStarted.get_future();
    auto waiterFinished = std::async(std::launch::async, [&]() {
        waiterStarted.set_value();
        proposal->waitUntilMyDecryptionSharesResolved();
    });

    waiterStartedFuture.wait();
    CATCH_REQUIRE(
        waiterFinished.wait_for(std::chrono::milliseconds(50)) == std::future_status::timeout);

    proposal->markMyDecryptionSharesFailed();

    CATCH_REQUIRE(
        waiterFinished.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    CATCH_REQUIRE(proposal->getMyDecryptionShares() == nullptr);
}

CATCH_TEST_CASE(
    "BlockProposal only starts decryption share computation once",
    "[bite][proposal][shares][async][single-flight]") {
    ConsensusEngine engine(0, 100000000);
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    auto cryptoManager = createTestCryptoManager(chain, node, engine);

    auto kp = generateKeys(1, 1);
    auto proposal = makeAsyncTestProposal(chain, cryptoManager, block_id(503), kp);

    CATCH_REQUIRE(proposal->tryBeginMyDecryptionSharesComputation());
    CATCH_REQUIRE_FALSE(proposal->tryBeginMyDecryptionSharesComputation());

    proposal->markMyDecryptionSharesFailed();

    CATCH_REQUIRE_FALSE(proposal->tryBeginMyDecryptionSharesComputation());
}

CATCH_TEST_CASE(
    "BiteManager schedules and persists local decryption shares",
    "[bite][manager][shares][async]") {
    ConsensusEngine engine(0, 100000000);
    shared_ptr<Schain> chain;
    shared_ptr<Node> node;
    auto cryptoManager = createTestCryptoManager(chain, node, engine);

    auto kp = generateKeys(1, 1);
    auto proposal = makeAsyncTestProposal(chain, cryptoManager, block_id(504), kp);
    auto biteManager = chain->getBiteManager();

    CATCH_REQUIRE(biteManager);

    biteManager->computeAndValidateSGXAESKeyBatch(proposal);
    CATCH_REQUIRE(proposal->getFailedTransactionsRef().empty());

    biteManager->scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(proposal);
    biteManager->scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(proposal);

    proposal->waitUntilMyDecryptionSharesResolved();

    auto proposalShares = proposal->getMyDecryptionShares();
    CATCH_REQUIRE(proposalShares != nullptr);

    auto dbShares = node->getTEDecryptionDB()->getMyDecryptionShares(
        proposal->getBlockID(), proposal->getProposerIndex());
    CATCH_REQUIRE(dbShares != nullptr);
    CATCH_REQUIRE(
        dbShares->totalCiphertextSharesCount() ==
        proposalShares->totalCiphertextSharesCount());
    CATCH_REQUIRE(dbShares->getBlockId() == proposal->getBlockID());
    CATCH_REQUIRE(dbShares->getProposerIndex() == proposal->getProposerIndex());
    CATCH_REQUIRE(dbShares->getDecryptorIndex() == chain->getSchainIndex());
}

#endif
