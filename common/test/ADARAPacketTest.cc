// Ported from Mantid (Framework/LiveData/test/ADARAPacketTest.h).
//
// Original Mantid header:
//   Copyright (c) 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source,
//     Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
//   SPDX-License-Identifier: GPL-3.0+
//
// This is a port of Mantid's cxxtest ADARAPacketTest suite to GoogleTest. The
// test exercises the ADARA stream parser (common/ADARAParser.{h,cc},
// common/ADARAPackets.{h,cc}) against a set of recorded sample packets. The
// sample packets live in ADARAPacketsTestData.h (the Mantid "data file",
// renamed so it does not collide with the real parser header ADARAPackets.h).

#include <gtest/gtest.h>

#include "ADARAParser.h"

#include <cassert>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// All of the sample packets that we need to run the tests are defined in the
// following header. The packets can get pretty long, which is why they don't
// clutter up this file.
#include "ADARAPacketsTestData.h"

// The test fixture inherits from ADARA::Parser, exactly as the original cxxtest
// suite did, so the test bodies (which are subclasses of this fixture) can reach
// the protected parser hooks (bufferFillAddress(), bufferBytesAppended(),
// bufferParse(), etc.).
class ADARAPacketTest : public ::testing::Test, public ADARA::Parser {
public:
  ADARAPacketTest()
      : ADARA::Parser(1024 * 1024, 1024 * 1024)
  // Set the initial buffer size equal to the max packet size.  This should
  // ensure that the parser will never resize its buffer.  See below for why
  // this is important.
  {
    // We know the parser's buffer is empty now, and we've ensured that the
    // address will never change.  Thus, we can verify that the buffer is empty
    // at any time in the future by calling bufferFillAddress() and comparing it
    // to this value.
    m_initialBufferAddr = bufferFillAddress();
  }

protected:
  // The rxPacket() functions just make a copy of the packet available in the
  // m_pkt member.  The test bodies handle everything from there.
  using ADARA::Parser::rxPacket;
#define DEFINE_RX_PACKET(PktType)                                              \
  bool rxPacket(const PktType &pkt) override {                                 \
    m_pkt.reset(new PktType(pkt));                                             \
    return false;                                                              \
  }

  DEFINE_RX_PACKET(ADARA::RawDataPkt)
  DEFINE_RX_PACKET(ADARA::RTDLPkt)
  DEFINE_RX_PACKET(ADARA::SourceListPkt)
  DEFINE_RX_PACKET(ADARA::BankedEventPkt)
  DEFINE_RX_PACKET(ADARA::BeamMonitorPkt)
  DEFINE_RX_PACKET(ADARA::PixelMappingAltPkt)
  DEFINE_RX_PACKET(ADARA::RunStatusPkt)
  DEFINE_RX_PACKET(ADARA::RunInfoPkt)
  DEFINE_RX_PACKET(ADARA::TransCompletePkt)
  DEFINE_RX_PACKET(ADARA::ClientHelloPkt)
  DEFINE_RX_PACKET(ADARA::AnnotationPkt)
  DEFINE_RX_PACKET(ADARA::SyncPkt)
  DEFINE_RX_PACKET(ADARA::HeartbeatPkt)
  DEFINE_RX_PACKET(ADARA::GeometryPkt)
  DEFINE_RX_PACKET(ADARA::BeamlineInfoPkt)
  DEFINE_RX_PACKET(ADARA::DetectorBankSetsPkt)
  DEFINE_RX_PACKET(ADARA::DataDonePkt)
  DEFINE_RX_PACKET(ADARA::DeviceDescriptorPkt)
  DEFINE_RX_PACKET(ADARA::VariableU32Pkt)
  DEFINE_RX_PACKET(ADARA::VariableDoublePkt)
  DEFINE_RX_PACKET(ADARA::VariableStringPkt)
  DEFINE_RX_PACKET(ADARA::VariableU32ArrayPkt)
  DEFINE_RX_PACKET(ADARA::VariableDoubleArrayPkt)
  DEFINE_RX_PACKET(ADARA::MultVariableU32Pkt)
  DEFINE_RX_PACKET(ADARA::MultVariableDoublePkt)
  DEFINE_RX_PACKET(ADARA::MultVariableStringPkt)
  DEFINE_RX_PACKET(ADARA::MultVariableU32ArrayPkt)
  DEFINE_RX_PACKET(ADARA::MultVariableDoubleArrayPkt)

  // Call the base class rxPacket(const ADARA::Packet &pkt) which will
  // eventually result in the execution of one of the rxPacket() functions
  // defined above.
  bool rxPacket(const ADARA::Packet &pkt) override { return ADARA::Parser::rxPacket(pkt); }

  unsigned char *m_initialBufferAddr;
  std::shared_ptr<ADARA::Packet> m_pkt;

  // A template function that covers the basic tests all packet types have to
  // pass.  Returns a shared pointer to the packet so further tests can be
  // conducted.
  //
  // NOTE: this helper returns a value, so it cannot use ASSERT_* (those expand
  // to `return;`).  It uses EXPECT_* and guards the deref-able checks with the
  // null test, exactly as the cxxtest original did.
  template <class T>
  std::shared_ptr<T> basicPacketTests(const unsigned char *data, unsigned len, unsigned pulseHigh, unsigned pulseLow) {
    parseOnePacket(data, len);

    // verify that we can cast the packet to the type we expect it to be
    std::shared_ptr<T> pkt = std::dynamic_pointer_cast<T>(m_pkt);
    EXPECT_NE(pkt, nullptr);

    // Make sure we have a valid packet before attempting the remaining tests
    if (pkt != nullptr) {
      EXPECT_EQ(pkt->packet_length(), len);
      EXPECT_EQ(pkt->payload_length(), len - sizeof(ADARA::Header));

      EXPECT_TRUE(pulseIdCompare(pkt->pulseId(), pulseHigh, pulseLow));
    }
    return pkt;
  }

  //
  // Helper functions for basicPacketTests()
  //

  // Calls the necessary parser functions to update m_pkt.  Expects a single
  // packet.  If there's more than one packet in len bytes, then this function
  // will fail an assertion.  m_pkt is updated by the rxPacket functions which
  // are called (eventually) from bufferParse().
  void parseOnePacket(const unsigned char *data, unsigned len) {
    m_pkt.reset();
    unsigned bufferLen = bufferFillLength();
    ASSERT_GT(bufferLen, 0u);
    ASSERT_GT(bufferLen, len);
    // Yes, len will always be greater than 0.  I want a specific warning if
    // dataLen is 0.

    unsigned char *bufferAddr = bufferFillAddress();
    ASSERT_NE(bufferAddr, nullptr);
    ASSERT_EQ(bufferAddr, m_initialBufferAddr); // verify that there's nothing in the buffer

    memcpy(bufferAddr, data, len);
    bufferBytesAppended(len);

    int packetsParsed = 0;
    std::string bufferParseLog;
    // bufferParse() wants a string where it can save log messages.
    // We don't actually use the messages for anything, though.
    ASSERT_NO_THROW((packetsParsed = bufferParse(bufferParseLog, 1)));
    ASSERT_EQ(packetsParsed, 1);
    ASSERT_NE(m_pkt, std::shared_ptr<ADARA::Packet>()); // verify m_pkt has been updated

    ASSERT_EQ(bufferParse(bufferParseLog, 0), 0);       // try to parse again, make sure there's nothing to parse
    ASSERT_EQ(bufferFillAddress(), m_initialBufferAddr); // verify that there's nothing in the buffer
  }

  // Compare the actual pulse ID value to the formatted value that is displayed
  // by running the adara parser.
  bool pulseIdCompare(uint64_t pulseId, uint32_t high, uint32_t low) {
    uint32_t pulseIdHigh = (uint32_t)(pulseId >> 32);
    uint32_t pulseIdLow = (uint32_t)(pulseId);
    return ((pulseIdHigh == high) && (pulseIdLow == low));
  }

  /**
   * Extract and cast a value from a packet buffer.
   *
   * @tparam T The type to cast the result to (uint16_t, uint32_t, etc)
   * @param packet The binary packet buffer
   * @param start Starting byte index
   * @param extent Number of bytes to read
   * @return The extracted value cast to type T
   */
  template <typename T> T packetCast(const unsigned char *packet, uint32_t start, uint32_t extent) {
    // Ensure the number of bytes is valid
    assert(extent > 0);
    assert(extent <= sizeof(T)); // Ensure we don't read more bytes than T can hold
    T result = 0;

    // Process bytes in little-endian order
    for (uint32_t i = 0; i < extent; ++i) {
      // Shift each byte to the appropriate position
      result |= static_cast<T>(packet[start + i]) << (i * 8);
    }

    return result;
  }
};

/*
 * The following packets are generated by the "Detector System", not the
 * "Stream Management Service".  As a result, they'll never be parsed by
 * Mantid's ADARA parser, and therefore Mantid didn't test them:
 *
 *   RawDataPkt
 *   MappedDataPkt
 *   RTDLPkt        (RTDL is tested below, however)
 *   SourceListPkt
 */

TEST_F(ADARAPacketTest, PacketCast) {
  const unsigned char data[8] = {0xd4, 0x48, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80};

  uint32_t result = packetCast<uint32_t>(data, 1, 4);
  EXPECT_EQ(result, 0x00000248u);

  // First 4 bytes should give 0x000248d4
  uint32_t result2 = packetCast<uint32_t>(data, 0, 4);
  EXPECT_EQ(result2, 0x000248d4u);

  // Test with shorter types
  uint16_t result3 = packetCast<uint16_t>(data, 1, 2);
  EXPECT_EQ(result3, 0x0248);

  // Test with a single byte
  uint8_t result4 = packetCast<uint8_t>(data, 0, 1);
  EXPECT_EQ(result4, 0xd4);

  // Test with offset at the end
  uint16_t result5 = packetCast<uint16_t>(data, 6, 2);
  EXPECT_EQ(result5, 0x8000);
}

TEST_F(ADARAPacketTest, BankedEventPacketV0Parser) {
  std::shared_ptr<ADARA::BankedEventPkt> pkt =
      basicPacketTests<ADARA::BankedEventPkt>(bankedEventPacketV0, sizeof(bankedEventPacketV0), 728504567, 761741666);
  if (pkt != nullptr) {
    EXPECT_EQ(pkt->cycle(), 0x3Cu);
    EXPECT_EQ(pkt->pulseCharge(), 1549703u);
    EXPECT_EQ(pkt->pulseEnergy(), 937987556u);
    EXPECT_EQ(pkt->flags(), 0u);

    const ADARA::Event *event = pkt->firstEvent();
    EXPECT_TRUE(event);
    if (event) {
      EXPECT_EQ(pkt->curBankId(), 0x02u);
      EXPECT_EQ(event->tof, 0x00023BD9u);
      EXPECT_EQ(event->pixel, 0x043Cu);
    }

    // This packet only has one event in its first bank, so fetch the
    // next event and verify the bank id
    event = pkt->nextEvent();
    EXPECT_TRUE(event);
    if (event) {
      EXPECT_EQ(pkt->curBankId(), 0x13u);
    }

    // There's also only one event in it's second (and last) bank.
    // Get the next event and verify it's null
    event = pkt->nextEvent();
    EXPECT_TRUE(!event);
  }
}

TEST_F(ADARAPacketTest, BankedEventPacketV1Parser) {
  std::shared_ptr<ADARA::BankedEventPkt> pkt =
      basicPacketTests<ADARA::BankedEventPkt>(bankedEventPacketV1, sizeof(bankedEventPacketV1), 1117010879, 984510667);

  // Returns a uint32_t value extracted from the packet at the given start offset (4 bytes)
  auto expectedAt = [&](uint32_t start) { return packetCast<uint32_t>(bankedEventPacketV1, start, 4); };

  if (pkt != nullptr) {
    // test packet header and some of the payload
    EXPECT_EQ(pkt->pulseCharge(), expectedAt(16));
    EXPECT_EQ(pkt->pulseEnergy(), expectedAt(20));
    EXPECT_EQ(pkt->cycle(), expectedAt(24));
    EXPECT_EQ(pkt->flags(), expectedAt(28) & 0xFFFFF);
    EXPECT_EQ(pkt->vetoFlags(), (expectedAt(28) >> 20) & 0xFFF);
    // inspect the first Source Section
    const ADARA::Event *event = pkt->firstEvent();
    EXPECT_TRUE(event);
    EXPECT_EQ(pkt->getSourceCORFlag(), false);
    EXPECT_EQ(pkt->getSourceTOFOffset(), 0x00000000u);
    EXPECT_EQ(pkt->curBankId(), expectedAt(48));
    EXPECT_EQ(pkt->curEventCount(), expectedAt(52));
    EXPECT_EQ(event->tof, expectedAt(56));
    EXPECT_EQ(event->pixel, expectedAt(60));

    // Advance to the last event of the first bank. There are 18 events in the first bank
    for (uint32_t i = 1; i < pkt->curEventCount(); ++i) {
      event = pkt->nextEvent();
      EXPECT_EQ(pkt->curBankId(), expectedAt(48)); // still in the first bank
      EXPECT_EQ(event->tof, expectedAt(56 + i * 8));
      EXPECT_EQ(event->pixel, expectedAt(60 + i * 8));
    }
    // Advance to the first event of the second bank
    event = pkt->nextEvent();
    EXPECT_TRUE(event);
    EXPECT_EQ(pkt->curBankId(), expectedAt(200));
    EXPECT_EQ(pkt->curEventCount(), expectedAt(204));
    EXPECT_EQ(event->tof, expectedAt(208));
    EXPECT_EQ(event->pixel, expectedAt(212));
  }
}

TEST_F(ADARAPacketTest, BeamMonitorPacketv0Parser) {
  std::shared_ptr<ADARA::BeamMonitorPkt> pkt =
      basicPacketTests<ADARA::BeamMonitorPkt>(beamMonitorPacketV0, sizeof(beamMonitorPacketV0), 728504567, 761741666);
  if (pkt != nullptr) {
    EXPECT_EQ(pkt->cycle(), 0x3cu);
    EXPECT_EQ(pkt->flags(), 0u);
    EXPECT_EQ(pkt->pulseCharge(), 1549703u);
    EXPECT_EQ(pkt->pulseEnergy(), 937987556u);
    // TODO: Find a different Beam Monitor Packet with actual monitor sections
    // in it
  }
}

TEST_F(ADARAPacketTest, BeamMonitorPacketv1Parser) {
  std::shared_ptr<ADARA::BeamMonitorPkt> pkt =
      basicPacketTests<ADARA::BeamMonitorPkt>(beamMonitorPacketV1, sizeof(beamMonitorPacketV1), 1117010871, 517706667);
  ASSERT_NE(pkt, nullptr);
  auto expectedAt = [&](uint32_t start) { return packetCast<uint32_t>(beamMonitorPacketV1, start, 4); };
  EXPECT_EQ(pkt->payload_length(), 136u - 16u); // 136 bytes total minus 16 bytes for the header
  EXPECT_EQ(pkt->pulseCharge(), expectedAt(16));
  EXPECT_EQ(pkt->pulseEnergy(), expectedAt(20));
  EXPECT_EQ(pkt->cycle(), expectedAt(24));
  EXPECT_EQ(pkt->flags(), ADARA::PARTIAL_DATA | ADARA::GOT_METADATA | ADARA::GOT_NEUTRONS);
  // Look into the first section
  EXPECT_TRUE(pkt->nextSection());
  EXPECT_EQ(pkt->getSectionEventCount(), expectedAt(32) & 0x003FFFFF); // lower 22 bits
  EXPECT_EQ(pkt->getSectionMonitorID(), expectedAt(32) >> 22);         // upper 10 bits
  EXPECT_EQ(pkt->getSectionSourceID(), expectedAt(36));
  EXPECT_EQ(pkt->getSectionTOFOffset(), expectedAt(40) & 0x7FFFFFFF);        // mask off the high bit
  EXPECT_EQ(pkt->sectionTOFCorrected(), (expectedAt(40) & 0x80000000) != 0); // only want the high bit
  // Look into the first event of the first section
  bool risingEdge;
  uint32_t cycle;
  uint32_t tof;
  pkt->nextEvent(risingEdge, cycle, tof);
  EXPECT_EQ(tof, expectedAt(44) & 0x001FFFFF);               // bits 20 to 0 (inclusive)
  EXPECT_EQ(cycle, (expectedAt(44) & 0x7FE00000) >> 21);     // bits 30 to 21 (inclusive)
  EXPECT_EQ(risingEdge, (expectedAt(44) & 0x80000000) != 0); // only want the high bit
  // There are 23 events in the first section, thus 44 + 23 * 4 = 136.
  // This packet has a total of 136 bytes, so there can't be another section.
  EXPECT_TRUE(!pkt->nextSection());
}

/*******************************************
 *   Pixel Mapping Table Alternate Packets
 *******************************************/

TEST_F(ADARAPacketTest, PixelMappingAltPktParserV0) {
  std::shared_ptr<ADARA::PixelMappingAltPkt> pkt = basicPacketTests<ADARA::PixelMappingAltPkt>(
      pixelMappingAltPktV0, sizeof(pixelMappingAltPktV0), 1112961350, 706131261);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->numBanks(), 1u);
}

TEST_F(ADARAPacketTest, PixelMappingAltPktParserV1direct) {
  std::shared_ptr<ADARA::PixelMappingAltPkt> pkt = basicPacketTests<ADARA::PixelMappingAltPkt>(
      pixelMappingAltPktV1direct, sizeof(pixelMappingAltPktV1direct), 1112961350, 706131261);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->numBanks(), 1u);
}

TEST_F(ADARAPacketTest, PixelMappingAltPktParserV1shorthand) {
  std::shared_ptr<ADARA::PixelMappingAltPkt> pkt = basicPacketTests<ADARA::PixelMappingAltPkt>(
      pixelMappingAltPktV1shorthand, sizeof(pixelMappingAltPktV1shorthand), 1112961350, 706131261);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->numBanks(), 1u);
}

/************************
 *   RUN STATUS Packets
 ************************/

TEST_F(ADARAPacketTest, RunStatusPacketParserV0) {
  std::shared_ptr<ADARA::RunStatusPkt> pkt =
      basicPacketTests<ADARA::RunStatusPkt>(runStatusPacketV0, sizeof(runStatusPacketV0), 728504568, 5625794);

  if (pkt != nullptr) {
    EXPECT_EQ(pkt->runNumber(), 13247u);
    EXPECT_EQ(pkt->runStart(), 728503297u);
    EXPECT_EQ(pkt->status(), ADARA::RunStatus::STATE);

    // TODO: Find a different RunStatus packet who's status is NOT STATE, then
    // check its file number
    // EXPECT_EQ(pkt->fileNumber(), ?????);
  }
}

TEST_F(ADARAPacketTest, RunStatusPacketParserV1) {
  // helper function to call on packets having different run status types
  auto testRunStatusPacket = [&](const unsigned char *packetData, unsigned int packetSize, uint32_t expectedPulseId,
                                 uint32_t expectedId, ADARA::RunStatus::Enum expectedStatus) {
    std::shared_ptr<ADARA::RunStatusPkt> pkt =
        basicPacketTests<ADARA::RunStatusPkt>(packetData, packetSize, expectedPulseId, expectedId);
    ASSERT_NE(pkt, nullptr);

    std::function<uint32_t(uint32_t)> expectedAt = [&](uint32_t start) {
      return packetCast<uint32_t>(packetData, start, 4);
    };

    EXPECT_EQ(pkt->runNumber(), expectedAt(16));
    EXPECT_EQ(pkt->runStart(), expectedAt(20));
    EXPECT_EQ(pkt->status(), expectedStatus);
    EXPECT_EQ(pkt->fileNumber(), expectedAt(24) & 0xFFFFFF); // lower 24 bits
  };

  testRunStatusPacket(runStatusPacketV1NoRun, sizeof(runStatusPacketV1NoRun), 1117010859, 421225535,
                      ADARA::RunStatus::NO_RUN);
  testRunStatusPacket(runStatusPacketV1NewRun, sizeof(runStatusPacketV1NewRun), 1117010897, 11143026,
                      ADARA::RunStatus::NEW_RUN);
  testRunStatusPacket(runStatusPacketV1RunEOF, sizeof(runStatusPacketV1RunEOF), 1117010900, 37361961,
                      ADARA::RunStatus::RUN_EOF);
  testRunStatusPacket(runStatusPacketV1RunBOF, sizeof(runStatusPacketV1RunBOF), 1117010897, 17020018,
                      ADARA::RunStatus::RUN_BOF);
  testRunStatusPacket(runStatusPacketV1EndRun, sizeof(runStatusPacketV1EndRun), 1117041153, 518160111,
                      ADARA::RunStatus::END_RUN);
}

/************************
 *   Run Info Packets
 ************************/

TEST_F(ADARAPacketTest, RunInfoPacketParserV0) {
  std::shared_ptr<ADARA::RunInfoPkt> pkt =
      basicPacketTests<ADARA::RunInfoPkt>(runInfoPacketV0, sizeof(runInfoPacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  const std::string expected = R"(<?xml version="1.0" encoding="UTF-8"?>
<root>Power to Run Info!</root>)";
  std::string result = pkt->info();
  // remove the null terminators from the end of the string,
  size_t nullPos = result.find_first_of('\0');
  if (nullPos != std::string::npos) {
    result.resize(nullPos);
  }
  EXPECT_EQ(result, expected);
}

/**********************************
 *   Translation Complete Packets
 *********************************/

TEST_F(ADARAPacketTest, TranslationCompletePacketParserV0) {
  std::shared_ptr<ADARA::TransCompletePkt> pkt = basicPacketTests<ADARA::TransCompletePkt>(
      translationCompletePacketV0, sizeof(translationCompletePacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(std::string(pkt->reason().c_str()), std::string("the meaning of the Universe"));
  EXPECT_EQ(pkt->status(), 42);
}

/*************************
 *   Client Hello Packets
 *************************/

TEST_F(ADARAPacketTest, ClientHelloPacketParserV0) {
  std::shared_ptr<ADARA::ClientHelloPkt> pkt = basicPacketTests<ADARA::ClientHelloPkt>(
      clientHelloPacketV0, sizeof(clientHelloPacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->requestedStartTime(), 42u);
}

TEST_F(ADARAPacketTest, ClientHelloPacketParserV1) {
  std::shared_ptr<ADARA::ClientHelloPkt> pkt = basicPacketTests<ADARA::ClientHelloPkt>(
      clientHelloPacketV1, sizeof(clientHelloPacketV1), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->requestedStartTime(), 42u);
  EXPECT_EQ(pkt->clientFlags(), ADARA::ClientHelloPkt::Flags::SEND_PAUSE_DATA);
}

/***********************
 *   Heartbeat Packets
 **********************/

TEST_F(ADARAPacketTest, HeartbeatPacketParserV0) {
  std::shared_ptr<ADARA::HeartbeatPkt> pkt =
      basicPacketTests<ADARA::HeartbeatPkt>(heartbeatPacketV0, sizeof(heartbeatPacketV0), 1117010859, 421225535);
  EXPECT_TRUE(pkt);
}

/**********************
 *   Geometry Packets
 *********************/

TEST_F(ADARAPacketTest, GeometryPacketParserV0) {
  std::shared_ptr<ADARA::GeometryPkt> pkt =
      basicPacketTests<ADARA::GeometryPkt>(geometryPacketV0, sizeof(geometryPacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(std::string(pkt->info().c_str()),
            std::string("<?xml version='1.0' encoding='ASCII'?>\n<instrument>VACUO</instrument>"));
}

/***************************
 *   Beamline Info Packets
 **************************/

TEST_F(ADARAPacketTest, BeamlineInfoPacketParserV0) {
  std::shared_ptr<ADARA::BeamlineInfoPkt> pkt = basicPacketTests<ADARA::BeamlineInfoPkt>(
      beamlineInfoPacketV0, sizeof(beamlineInfoPacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->id(), "42");
  EXPECT_EQ(pkt->shortName(), "CG3");
  EXPECT_EQ(pkt->longName(), "BIOSANS");
}

TEST_F(ADARAPacketTest, BeamlineInfoPacketParserV1) {
  std::shared_ptr<ADARA::BeamlineInfoPkt> pkt = basicPacketTests<ADARA::BeamlineInfoPkt>(
      beamlineInfoPacketV1, sizeof(beamlineInfoPacketV1), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->targetStationNumber(), 2u);
  EXPECT_EQ(pkt->id(), "42");
  EXPECT_EQ(pkt->shortName(), "CG3");
  EXPECT_EQ(pkt->longName(), "BIOSANS");
}

/********************************
 *   Detector Bank Sets Packets
 *******************************/

TEST_F(ADARAPacketTest, DectectorBankSetPacketParserV0) {
  std::shared_ptr<ADARA::DetectorBankSetsPkt> pkt = basicPacketTests<ADARA::DetectorBankSetsPkt>(
      detectorBankSetPacketV0, sizeof(detectorBankSetPacketV0), 1117010859, 421225535);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->detBankSetCount(), 1u);
  EXPECT_EQ(pkt->name(0), "front row");
  EXPECT_EQ(pkt->flags(0), 1u);
  EXPECT_EQ(pkt->bankCount(0), 3u);
  EXPECT_EQ(pkt->tofOffset(0), 420u);
  EXPECT_EQ(pkt->tofMax(0), 16667u);
  EXPECT_EQ(pkt->tofBin(0), 10u);
  EXPECT_EQ(pkt->throttle(0), 0.0);
  EXPECT_EQ(pkt->suffix(0), "throttled");
}

/***********************
 *   Data Done Packets
 **********************/

TEST_F(ADARAPacketTest, DataDonePacketParserV0) {
  std::shared_ptr<ADARA::DataDonePkt> pkt =
      basicPacketTests<ADARA::DataDonePkt>(dataDonePacketV0, sizeof(dataDonePacketV0), 1117010859, 421225535);
  EXPECT_TRUE(pkt);
}

/************************
 *   RTDL Packets
 ************************/

TEST_F(ADARAPacketTest, RTDLPacketParserV0) {
  std::shared_ptr<ADARA::RTDLPkt> pkt =
      basicPacketTests<ADARA::RTDLPkt>(rtdlPacketV0, sizeof(rtdlPacketV0), 728504567, 761741666);

  if (pkt != nullptr) {
    EXPECT_EQ(pkt->cycle(), 60u);
    EXPECT_EQ(pkt->vetoFlags(), 0x4u);
    EXPECT_EQ(pkt->badVeto(), false);
    EXPECT_EQ(pkt->timingStatus(), 0x1eu);
    EXPECT_EQ(pkt->flavor(), 1);
    EXPECT_EQ(pkt->intraPulseTime(), 166662u);
    EXPECT_EQ(pkt->tofOffset(), 63112u);
    EXPECT_EQ(pkt->pulseCharge(), 1549703u);
    EXPECT_EQ(pkt->ringPeriod(), 955259u);
  }
}

TEST_F(ADARAPacketTest, RTDLPacketParserV1) {
  std::shared_ptr<ADARA::RTDLPkt> pkt =
      basicPacketTests<ADARA::RTDLPkt>(rtdlPacketV1, sizeof(rtdlPacketV1), 728504567, 761741666);
  ASSERT_NE(pkt, nullptr);
  auto expectedAt = [&](uint32_t start) { return packetCast<uint32_t>(rtdlPacketV1, start, 4); };

  uint32_t quadByte = expectedAt(16);
  EXPECT_EQ(pkt->gotDataFlags(), true);                 // bit index 31
  EXPECT_EQ(pkt->dataFlags(), (quadByte >> 27) & 0x1f); // bit indexes 27 to 31
  EXPECT_EQ(pkt->flavor(), ADARA::PulseFlavor::NORMAL);
  EXPECT_EQ(pkt->pulseCharge(), quadByte & 0x00ffffff); // bit indexes 0 to 23

  quadByte = expectedAt(20);
  EXPECT_EQ(pkt->badVeto(), (bool)(quadByte & 0x80000000));          // bit index 31
  EXPECT_EQ(pkt->badCycle(), (bool)(quadByte & 0x40000000));         // bit index 30
  EXPECT_EQ(pkt->timingStatus(), (uint8_t)(quadByte >> 22));         // bit indexes 22 to 29
  EXPECT_EQ(pkt->vetoFlags(), (uint16_t)((quadByte >> 10) & 0xfff)); // bit indexes 10 to 21
  EXPECT_EQ(pkt->cycle(), (uint16_t)(quadByte & 0x3ff));

  EXPECT_EQ(pkt->intraPulseTime(), expectedAt(24));
  EXPECT_EQ(pkt->tofCorrected(), (bool)(expectedAt(28) & 0x80000000)); // bit index 31
  EXPECT_EQ(pkt->tofOffset(), expectedAt(28) & 0x7fffffff);            // mask off the high bit
  EXPECT_EQ(pkt->ringPeriod(), expectedAt(32) & 0xffffff);             // bit indexes 0 to 23

  EXPECT_EQ(pkt->FNA(0), (expectedAt(36) >> 24) & 0xff);      // bit indexes 0 to 23
  EXPECT_EQ(pkt->frameData(0), expectedAt(36) & 0xffffff);    // bit indexes 24 to 31
  EXPECT_EQ(pkt->FNA(24), (expectedAt(132) >> 24) & 0xff);    // bit indexes 0 to 23
  EXPECT_EQ(pkt->frameData(24), expectedAt(132) & 0xffffff);  // bit indexes 24 to 31
}

/************************
 *   SYNC Packets
 *   (there's only version 0)
 ************************/

TEST_F(ADARAPacketTest, SyncPacketParser) {
  std::shared_ptr<ADARA::SyncPkt> pkt =
      basicPacketTests<ADARA::SyncPkt>(syncPacket, sizeof(syncPacket), 728504568, 5617153);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(std::string(pkt->signature().c_str()), std::string("SNSADARAORNL"));
  EXPECT_EQ(pkt->fileOffset(), 42u);
  EXPECT_EQ(pkt->comment().length(), 0u); // empty comment
}

/******************************
 *   STREAM ANNOTATION Packets
 *   (there's only version 0)
 ********************************/

TEST_F(ADARAPacketTest, AnnotationPacketParser) {
  std::shared_ptr<ADARA::AnnotationPkt> pkt = basicPacketTests<ADARA::AnnotationPkt>(
      AnnotationPacketType0, sizeof(AnnotationPacketType0), 1117010897, 8968804);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::GENERIC);
  EXPECT_EQ(pkt->scanIndex(), 0u);
  EXPECT_EQ(pkt->comment(), "Run 44635 Started.");

  pkt = basicPacketTests<ADARA::AnnotationPkt>(AnnotationPacketType1, sizeof(AnnotationPacketType1), 1117010982,
                                               114515313);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::SCAN_START);
  EXPECT_EQ(pkt->scanIndex(), 2u);
  EXPECT_EQ(pkt->comment(), "[Run 44635] Scan #2 Started.");

  pkt = basicPacketTests<ADARA::AnnotationPkt>(AnnotationPacketType2, sizeof(AnnotationPacketType2), 1117011060,
                                               417136782);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::SCAN_STOP);
  EXPECT_EQ(pkt->scanIndex(), 2u);
  EXPECT_EQ(pkt->comment(), "[Run 44635] Scan #2 Stopped.");

  pkt = basicPacketTests<ADARA::AnnotationPkt>(AnnotationPacketType3, sizeof(AnnotationPacketType3), 1117010897,
                                               14178350);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::PAUSE);
  EXPECT_EQ(pkt->scanIndex(), 0u);
  EXPECT_EQ(pkt->comment(), "Run 44635 Paused.");

  pkt = basicPacketTests<ADARA::AnnotationPkt>(AnnotationPacketType4, sizeof(AnnotationPacketType4), 1117010897,
                                               16250000);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::RESUME);
  EXPECT_EQ(pkt->scanIndex(), 0u);
  EXPECT_EQ(pkt->comment(), "Run 44635 Resumed.");

  pkt = basicPacketTests<ADARA::AnnotationPkt>(AnnotationPacketType5, sizeof(AnnotationPacketType5), 1117010859,
                                               418887449);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->marker_type(), ADARA::MarkerType::OVERALL_RUN_COMMENT);
  EXPECT_EQ(pkt->scanIndex(), 0u);
  EXPECT_EQ(pkt->comment(), "(unset)");
}

/******************************
 *   VARIABLE VALUE Packets
 *   (there's only version 0)
 ********************************/

TEST_F(ADARAPacketTest, VariableU32PacketParser) {
  std::shared_ptr<ADARA::VariableU32Pkt> pkt =
      basicPacketTests<ADARA::VariableU32Pkt>(variableU32Packet, sizeof(variableU32Packet), 728281149, 0);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 2u);
  EXPECT_EQ(pkt->varId(), 3u);
  EXPECT_EQ(pkt->status(), 0);
  EXPECT_EQ(pkt->severity(), 0);
  EXPECT_EQ(pkt->value(), 3u);
}

TEST_F(ADARAPacketTest, VariableDoublePacketParser) {
  std::shared_ptr<ADARA::VariableDoublePkt> pkt =
      basicPacketTests<ADARA::VariableDoublePkt>(variableDoublePacket, sizeof(variableDoublePacket), 728281149, 0);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 2u);
  EXPECT_EQ(pkt->varId(), 1u);
  EXPECT_EQ(pkt->status(), 0);
  EXPECT_EQ(pkt->severity(), 0);
  EXPECT_NEAR(pkt->value(), 5.0015, 1e-8);
}

TEST_F(ADARAPacketTest, VariableStringPacketParser) {
  std::shared_ptr<ADARA::VariableStringPkt> pkt = basicPacketTests<ADARA::VariableStringPkt>(
      variableStringPacketValue1, sizeof(variableStringPacketValue1), 1116514626, 726460216);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 17u);
  EXPECT_EQ(pkt->varId(), 3u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->value(), "N/A");

  pkt = basicPacketTests<ADARA::VariableStringPkt>(variableStringPacketValue2, sizeof(variableStringPacketValue2),
                                                   1116514626, 726434341);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 17u);
  EXPECT_EQ(pkt->varId(), 2u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->value(), "No sample");

  pkt = basicPacketTests<ADARA::VariableStringPkt>(variableStringPacketValue3, sizeof(variableStringPacketValue3),
                                                   1112954908, 544477614);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 7u);
  EXPECT_EQ(pkt->varId(), 1u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->value(), "'SampleTemp', 'SampleTempSelect', 'MagneticField', 'MagneticFieldSelector', "
                          "'sequence_id', 'sequence_number', 'sequence_total', 'SF1', 'SF2'");
}

TEST_F(ADARAPacketTest, VariableU32ArrayPacketParser) {
  std::shared_ptr<ADARA::VariableU32ArrayPkt> pkt = basicPacketTests<ADARA::VariableU32ArrayPkt>(
      variableU32ArrayPacket, sizeof(variableU32ArrayPacket), 728281149, 0);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 2u);
  EXPECT_EQ(pkt->varId(), 3u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->elemCount(), 4u);
  EXPECT_EQ(pkt->value(), std::vector<uint32_t>({6, 7, 8, 9}));
}

TEST_F(ADARAPacketTest, VariableDoubleArrayPacketParser) {
  std::shared_ptr<ADARA::VariableDoubleArrayPkt> pkt = basicPacketTests<ADARA::VariableDoubleArrayPkt>(
      variableDoubleArrayPacket, sizeof(variableDoubleArrayPacket), 728281149, 0);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 2u);
  EXPECT_EQ(pkt->varId(), 1u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->elemCount(), 2u);
  for (size_t i = 0; i < pkt->elemCount(); i++)
    EXPECT_NEAR(pkt->value()[i], static_cast<double>(i) + 8.0, 1.e-8);
}

TEST_F(ADARAPacketTest, MultVariableU32PacketParser) {
  std::shared_ptr<ADARA::MultVariableU32Pkt> pkt = basicPacketTests<ADARA::MultVariableU32Pkt>(
      multVariableU32Packet, sizeof(multVariableU32Packet), 1117010982, 102843667);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 1u);
  EXPECT_EQ(pkt->varId(), 11u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->numValues(), 16u);
  std::vector<uint32_t> tofs = {17067500, 22171400, 22172800, 22203900, 22207000, 22237400, 22240900, 22255100,
                                22260900, 22270700, 22278400, 22287600, 22297700, 22303500, 22314900, 22319400};
  std::vector<uint32_t> values = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
  for (size_t i = 0; i < pkt->numValues(); i++) {
    EXPECT_EQ(pkt->tofs()[i], tofs[i]);
    EXPECT_EQ(pkt->values()[i], values[i]);
  }
}

TEST_F(ADARAPacketTest, MultVariableDoublePacketParser) {
  std::shared_ptr<ADARA::MultVariableDoublePkt> pkt = basicPacketTests<ADARA::MultVariableDoublePkt>(
      multVariableDoublePacket, sizeof(multVariableDoublePacket), 1117010982, 102843667);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 1u);
  EXPECT_EQ(pkt->varId(), 11u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->numValues(), 8u);
  std::vector<uint32_t> tofs = {17067500, 22171400, 22172800, 22203900, 22207000, 22237400, 22240900, 22255100};
  std::vector<double> values_expected = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
  for (size_t i = 0; i < pkt->numValues(); i++) {
    EXPECT_EQ(pkt->tofs()[i], tofs[i]);
    EXPECT_NEAR(pkt->values()[i], values_expected[i], 1.e-8);
  }
}

TEST_F(ADARAPacketTest, MultVariableStringPacketParser) {
  std::shared_ptr<ADARA::MultVariableStringPkt> pkt = basicPacketTests<ADARA::MultVariableStringPkt>(
      multVariableStringPacket, sizeof(multVariableStringPacket), 1117010982, 102843667);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 1u);
  EXPECT_EQ(pkt->varId(), 11u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->numValues(), 3u);
  std::vector<uint32_t> tofs = {17067500, 22171400, 22172800, 22203900, 22207000, 22237400, 22240900, 22255100};
  std::vector<std::string> values = {"one", "two", "three"};
  for (size_t i = 0; i < pkt->numValues(); i++) {
    EXPECT_EQ(pkt->tofs()[i], tofs[i]);
    EXPECT_EQ(std::string(pkt->values()[i].c_str()), values[i]);
  }
}

TEST_F(ADARAPacketTest, MultVariableU32ArrayParser) {
  std::shared_ptr<ADARA::MultVariableU32ArrayPkt> pkt = basicPacketTests<ADARA::MultVariableU32ArrayPkt>(
      multVariableU32ArrayPacket, sizeof(multVariableU32ArrayPacket), 1117010982, 102843667);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 1u);
  EXPECT_EQ(pkt->varId(), 11u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->numValues(), 3u);
  std::vector<uint32_t> tofs = {17067500, 22171400, 22172800};
  std::vector<uint32_t> element_counts = {2, 1, 2};
  std::vector<std::vector<uint32_t>> values_expected = {{1, 2}, {3}, {4, 5}};
  for (size_t i = 0; i < pkt->numValues(); i++) {
    EXPECT_EQ(pkt->tofs()[i], tofs[i]);
    for (size_t j = 0; j < element_counts[i]; ++j)
      EXPECT_EQ(pkt->values()[i][j], values_expected[i][j]);
  }
}

TEST_F(ADARAPacketTest, MultVariableDoubleArrayParser) {
  std::shared_ptr<ADARA::MultVariableDoubleArrayPkt> pkt = basicPacketTests<ADARA::MultVariableDoubleArrayPkt>(
      multVariableDoubleArrayPacket, sizeof(multVariableDoubleArrayPacket), 1117010982, 102843667);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->devId(), 1u);
  EXPECT_EQ(pkt->varId(), 11u);
  EXPECT_EQ(pkt->status(), ADARA::VariableStatus::OK);
  EXPECT_EQ(pkt->severity(), ADARA::VariableSeverity::OK);
  EXPECT_EQ(pkt->numValues(), 3u);
  std::vector<uint32_t> tofs = {17067500, 22171400, 22172800};
  std::vector<uint32_t> element_counts = {2, 1, 2};
  std::vector<std::vector<double>> values_expected = {{1.0, 2.0}, {3.0}, {4.0, 5.0}};
  for (size_t i = 0; i < pkt->numValues(); i++) {
    EXPECT_EQ(pkt->tofs()[i], tofs[i]);
    for (size_t j = 0; j < element_counts[i]; ++j)
      EXPECT_NEAR(pkt->values()[i][j], values_expected[i][j], 1e-8);
  }
}

/*******************************
 *   DEVICE DESCRIPTOR Packets
 *******************************/

// There's only version 0 for the DeviceDescriptorPkt, so we don't need a
// version-specific test.
TEST_F(ADARAPacketTest, DeviceDescriptorPacketParser) {
  std::shared_ptr<ADARA::DeviceDescriptorPkt> pkt =
      basicPacketTests<ADARA::DeviceDescriptorPkt>(devDesPacket, sizeof(devDesPacket), 726785379, 0);
  ASSERT_NE(pkt, nullptr);
  auto expectedAt = [&](uint32_t start) { return packetCast<uint32_t>(devDesPacket, start, 4); };
  EXPECT_EQ(pkt->devId(), expectedAt(16));
  // Mantid validates the description with a Poco DOM parser. adara has no Poco
  // dependency, so we just sanity-check that the payload looks like XML.
  const std::string desc = pkt->description();
  EXPECT_FALSE(desc.empty());
  EXPECT_NE(desc.find('<'), std::string::npos);
}
