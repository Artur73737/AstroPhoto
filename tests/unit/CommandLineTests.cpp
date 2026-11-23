#include <astro/app/command_line/Options.hpp>
#include <gtest/gtest.h>
#include <stdexcept>
using namespace astro::app::command_line;
TEST(CommandLine,RecognizesEveryBundledCompassDemoWithoutConnectingCamera){for(int n:{1,2,3}){RunOptions o;parseArguments({"--mcp-http","--true-north-demo="+std::to_string(n)},o);EXPECT_EQ(o.northDemo,n);EXPECT_EQ(o.mcpTransport,"http");EXPECT_FALSE(o.connect);}}
TEST(CommandLine,RejectsUnknownOutOfRangeAndPartialNumbers){for(auto arg:{"--true-north-demo=0","--true-north-demo=4","--true-north-demo=1garbage","--true-north-demo=","--unknown"}){RunOptions o;EXPECT_THROW(parseArguments({arg},o),std::invalid_argument);}}
TEST(CommandLine,PreservesAUnicodePathWithSpacesAsOneArgument){RunOptions o;parseArguments({"--pairing-test-file=E:/folder with spaces/à-file.txt","--live-view"},o);EXPECT_EQ(o.pairingTestFile.filename(),std::filesystem::u8path("à-file.txt"));EXPECT_TRUE(o.connect);EXPECT_TRUE(o.startLive);}
TEST(CommandLine,TransportSelectionRemainsExclusive){RunOptions o;EXPECT_THROW(parseArguments({"--mcp-http","--mcp-stdio"},o),std::invalid_argument);}
