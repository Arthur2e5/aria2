#include "UTMetadataPostDownloadHandler.h"

#include <cppunit/extensions/HelperMacros.h>

#include "DownloadContext.h"
#include "RequestGroup.h"
#include "Option.h"
#include "FileEntry.h"
#include "bittorrent_helper.h"
#include "A2STR.h"
#include "ByteArrayDiskWriterFactory.h"
#include "PieceStorage.h"
#include "DiskAdaptor.h"
#include "util.h"
#include "MessageDigestHelper.h"

namespace aria2 {

class UTMetadataPostDownloadHandlerTest:public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(UTMetadataPostDownloadHandlerTest);
  CPPUNIT_TEST(testCanHandle);
  CPPUNIT_TEST(testGetNextRequestGroups);
  CPPUNIT_TEST_SUITE_END();
private:
  SharedHandle<Option> _option;
  SharedHandle<DownloadContext> _dctx;
  SharedHandle<RequestGroup> _requestGroup;
public:
  void setUp()
  {
    _option.reset(new Option());
    _option->put("HELLO", "WORLD");
    _dctx.reset(new DownloadContext(0, 0, "something"));
    _requestGroup.reset(new RequestGroup(_option));
    _requestGroup->setDownloadContext(_dctx);
  }

  void testCanHandle();
  void testGetNextRequestGroups();
};


CPPUNIT_TEST_SUITE_REGISTRATION( UTMetadataPostDownloadHandlerTest );

void UTMetadataPostDownloadHandlerTest::testCanHandle()
{
  UTMetadataPostDownloadHandler handler;

  CPPUNIT_ASSERT(!handler.canHandle(_requestGroup.get()));

  BDE attrs = BDE::dict();
  _dctx->setAttribute(bittorrent::BITTORRENT, attrs);

  CPPUNIT_ASSERT(handler.canHandle(_requestGroup.get()));

  // Only checks existence of METADATA key
  attrs[bittorrent::METADATA] = A2STR::NIL;

  CPPUNIT_ASSERT(!handler.canHandle(_requestGroup.get()));
}

void UTMetadataPostDownloadHandlerTest::testGetNextRequestGroups()
{
  std::string metadata =
    "d6:lengthi384e4:name19:aria2-0.8.2.tar.bz212:piece lengthi128e"
    "6:pieces60:AAAAAAAAAAAAAAAAAAAABBBBBBBBBBBBBBBBBBBBCCCCCCCCCCCCCCCCCCCCe";
  unsigned char infoHash[20];
  MessageDigestHelper::digest
    (infoHash, sizeof(infoHash), MessageDigestContext::SHA1,
     reinterpret_cast<const unsigned char*>(metadata.data()), metadata.size());
  _dctx->getFirstFileEntry()->setLength(metadata.size());
  BDE attrs = BDE::dict();
  attrs[bittorrent::INFO_HASH] = std::string(&infoHash[0], &infoHash[20]);
  BDE announceList = BDE::list();
  attrs[bittorrent::ANNOUNCE_LIST] = announceList;
  _dctx->setAttribute(bittorrent::BITTORRENT, attrs);
  _requestGroup->setDiskWriterFactory
    (SharedHandle<DiskWriterFactory>(new ByteArrayDiskWriterFactory()));
  _requestGroup->initPieceStorage();
  _requestGroup->getPieceStorage()->getDiskAdaptor()->writeData
    (reinterpret_cast<const unsigned char*>(metadata.data()), metadata.size(),
     0);

  UTMetadataPostDownloadHandler handler;
  std::deque<SharedHandle<RequestGroup> > results;
  handler.getNextRequestGroups(results, _requestGroup.get());

  CPPUNIT_ASSERT_EQUAL((size_t)1, results.size());
  SharedHandle<RequestGroup> newRg = results.front();
  SharedHandle<DownloadContext> newDctx = newRg->getDownloadContext();
  const BDE& newAttrs = newDctx->getAttribute(bittorrent::BITTORRENT);
  CPPUNIT_ASSERT_EQUAL(util::toHex(attrs[bittorrent::INFO_HASH].s()),
		       util::toHex(newAttrs[bittorrent::INFO_HASH].s()));
  CPPUNIT_ASSERT(newAttrs.containsKey(bittorrent::ANNOUNCE_LIST));
  CPPUNIT_ASSERT_EQUAL(_option->get("Hello"),
		       newRg->getOption()->get("Hello"));

  results.clear();

  // See failure with bad metadata
  metadata = "d6:lengthi384e4:name19:aria2-0.8.2.tar.bz212:piece lengthi128e";
  _requestGroup->initPieceStorage();
  _requestGroup->getPieceStorage()->getDiskAdaptor()->writeData
    (reinterpret_cast<const unsigned char*>(metadata.data()), metadata.size(),
     0);
  handler.getNextRequestGroups(results, _requestGroup.get());
  CPPUNIT_ASSERT(results.empty());  
}

} // namespace aria2