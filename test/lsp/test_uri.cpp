#include <test/test.h>
#include <lsp/uri.h>
#include <filesystem>

using namespace lsp;

int main(int argc, char** argv)
{
	auto app = test::TestApp();

	app.addTest("Scheme", [](std::string_view input, std::string_view expectedScheme)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.scheme(), expectedScheme);
	})({
		{"SchemeOnly",          {"file:",    "file"}},
		{"SchemeAuthority",     {"file://",  "file"}},
		{"SchemeAuthorityPath", {"file:///", "file"}},
		{"SchemeNonAlpha",      {"a1-.+:",   "a1-.+"}},
	});

	app.addTest("Authority", [](std::string_view input, std::string_view expectedAuthority, bool expectedHasAuthority)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.hasAuthority(), expectedHasAuthority);
		test::compare(uri.authority(), expectedAuthority);
	})({
		{"NoAuthority",               {"file:",                                  {},                           false}},
		{"NoAuthorityPath",           {"file:/",                                 {},                           false}},
		{"EmptyAuthority",            {"file://",                                {},                           true}},
		{"EmptyAuthorityPath",        {"file:///",                               {},                           true}},
		{"AuthorityNoPath",           {"file://server",                          "server",                     true}},
		{"AuthorityPath",             {"file://server/share/file.txt",           "server",                     true}},
		{"AuthorityUserInfoPort",     {"http://user:pass@example.com:8080",      "user:pass@example.com:8080", true}},
		{"AuthorityUserInfoPortPath", {"http://user:pass@example.com:8080/path", "user:pass@example.com:8080", true}},
	});

	app.addTest("Path", [](std::string_view input, std::string_view expectedPath)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.path(), expectedPath);
	})({
		{"Empty",                    {"file:",               {}               }},
		{"RootNoAuthority",          {"file:/",              "/"              }},
		{"EmptyAuthority",           {"file://",             {}               }},
		{"RootWithAuthority",        {"file:///",            "/"              }},
		{"MultiSegment",             {"file:///a/b/c",       "/a/b/c"         }},
		{"OpaquePath",               {"urn:isbn:0451450523", "isbn:0451450523"}},
		{"PercentDecoded",           {"file:///a%20b",       "/a b"           }},
		{"PercentDecodedUnicode",    {"file:///%E4%B8%AD",   "/中"            }},
		{"MalformedPercentEncoding", {"file:///%zz",         {}               }},
		{"QueryTerminatesPath",      {"file:///path?query",  "/path"          }},
		{"FragmentTerminatesPath",   {"file:///path#frag",   "/path"          }},
	});

	app.addTest("Query", [](std::string_view input, std::string_view expectedQuery, bool expectedHasQuery)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.hasQuery(), expectedHasQuery);
		test::compare(uri.query(), expectedQuery);
	})({
		{"NoQuery",                   {"file:///path",               {},          false}},
		{"EmptyQuery",                {"file:///path?",              "",          true }},
		{"Query",                     {"file:///path?key=value",     "key=value", true }},
		{"QueryNoPath",               {"custom:?key=value",          "key=value", true }},
		{"FragmentTerminatesQuery",   {"file:///path?key=value#sec", "key=value", true }},
		{"NotPercentDecoded",         {"file:///path?a%20b",         "a%20b",     true }},
		{"PercentEncodingUppercased", {"file:///path?a%2fb",         "a%2Fb",     true }},
		{"EmbeddedQuestionMark",      {"file:///path?a=1?b=2",       "a=1?b=2",   true }},
	});

	app.addTest("Fragment", [](std::string_view input, std::string_view expectedFragment, bool expectedHasFragment)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.hasFragment(), expectedHasFragment);
		test::compare(uri.fragment(), expectedFragment);
	})({
		{"NoFragment",                {"file:///path",                   {},        false}},
		{"EmptyFragment",             {"file:///path#",                  "",        true }},
		{"Fragment",                  {"file:///path#section",           "section", true }},
		{"FragmentNoPath",            {"custom:#section",                "section", true }},
		{"FragmentAfterQuery",        {"file:///path?key=value#section", "section", true }},
		{"NotPercentDecoded",         {"file:///path#a%20b",             "a%20b",   true }},
		{"PercentEncodingUppercased", {"file:///path#a%2fb",             "a%2Fb",   true }},
		{"EmbeddedHash",              {"file:///path#a#b",               "a#b",     true }},
		{"EmbeddedQuestionMark",      {"file:///path#a?b",               "a?b",     true }},
	});

	app.addTest("Invalid", [](std::string_view input)
	{
		const auto uri = Uri::parse(input);
		test::check(!uri.isValid());
	})({
		{"Empty",                              {""}},
		{"NoColon",                            {"noscheme"}},
		{"EmptyScheme",                        {"://path"}},
		{"InvalidSchemeChar",                  {"ht!tp:path"}},
		{"NoSchemeAuthorityLike",              {"//host/path"}},
		{"AuthorityWithoutPathBeforeQuery",    {"http://example.com?query"}},
		{"AuthorityWithoutPathBeforeFragment", {"http://example.com#frag"}},
	});

	app.addTest("Set/Remove", [](){
		auto uri = Uri::parse("http://example.com/path?query#fragment");
		test::check(uri.isValid());

		test::check(uri.setScheme("HTTPS"), "setScheme"); // also lowercased
		test::compare(uri.scheme(), "https");

		test::check(!uri.setScheme("ht!tp"), "setSchemeInvalidRejected");
		test::compare(uri.scheme(), "https"); // unchanged

		test::check(!uri.setScheme("1http"), "setSchemeInvalidNonAlphaRejected");
		test::compare(uri.scheme(), "https"); // unchanged

		test::check(uri.setAuthority("server"), "setAuthority");
		test::compare(uri.authority(), "server");

		test::check(!uri.setAuthority("s#rver"), "setAuthorityInvalidRejected");
		test::compare(uri.authority(), "server"); // unchanged

		// setPath doesn't validate its input - reserved characters just get percent-encoded on output
		test::check(uri.setPath("/a?b#c"), "setPathNoValidation");
		test::compare(uri.toString(), "https://server/a%3Fb%23c?query#fragment");

		uri.setPath("/path"); // restore a normal path for the remaining steps
		test::compare(uri.toString(), "https://server/path?query#fragment");

		test::check(uri.setQuery("key=value"), "setQuery");
		test::compare(uri.query(), "key=value");

		test::check(!uri.setQuery("a#b"), "setQueryInvalidRejected");
		test::compare(uri.query(), "key=value"); // unchanged

		test::check(uri.setFragment("a?b#c"), "setFragmentNoValidation");
		test::compare(uri.toString(), "https://server/path?key=value#a?b#c");

		uri.removeFragment();
		test::check(!uri.hasFragment(), "removeFragment");
		test::compare(uri.toString(), "https://server/path?key=value");

		uri.removeQuery();
		test::check(!uri.hasQuery(), "removeQuery");
		test::compare(uri.toString(), "https://server/path");

		uri.removeQuery(); // no-op, already absent
		test::check(!uri.hasQuery(), "removeQueryWhenAbsentIsNoop");

		uri.removeAuthority();
		test::check(!uri.hasAuthority(), "removeAuthority");
		test::compare(uri.toString(), "https:/path");

		// Spaces are currently not checked and will just be written.
		// Might fix that in the future but it doesn't really matter for any use case.
		test::check(uri.setQuery("a b"), "setQueryAcceptsRawSpace");
		test::compare(uri.toString(), "https:/path?a b");
	});

	app.addTest("ToString", [](){
		auto uri = Uri();
		test::compare(uri.toString(), ""); // no scheme -> invalid -> empty string

		uri.setScheme("file");
		test::compare(uri.toString(), "file:");

		uri.setPath("/a b");
		test::compare(uri.toString(), "file:/a%20b"); // no authority set -> no "//", and the space gets percent-encoded

		uri.setAuthority(""); // explicitly setting an empty authority still turns on hasAuthority
		test::compare(uri.toString(), "file:///a%20b");

		uri.setAuthority("user:pass@example.com:8080");
		test::compare(uri.toString(), "file://user:pass@example.com:8080/a%20b");

		uri.setQuery("key=value");
		test::compare(uri.toString(), "file://user:pass@example.com:8080/a%20b?key=value");

		uri.setFragment("section");
		const std::string_view full = "file://user:pass@example.com:8080/a%20b?key=value#section";
		test::compare(uri.toString(), full);

		test::compare(Uri::parse(full).toString(), full);

		uri.removeAuthority();
		test::compare(uri.toString(), "file:/a%20b?key=value#section"); // no authority -> no "//"

		uri.setScheme("file");
		uri.setPath("C:/some/file");
		uri.setAuthority({});
		test::compare(uri.toString(), "file:///C%3A/some/file?key=value#section");
	});

	app.addTest("IsFileUri", [](std::string_view input, bool expected)
	{
		const auto uri = Uri::parse(input);
		test::check(uri.isValid());
		test::compare(uri.isFileUri(), expected);
	})({
		{"FileScheme",    {"file:///path",       true }},
		{"NonFileScheme", {"http://example.com", false}},
	});

	app.addTest("FileUriFromPath/FsPath", [](){
		auto expectedAbsolute = [](std::string_view path)
		{
			return std::filesystem::absolute(path).string();
		};

		{
			const auto input = "relative/path";
			const auto uri    = Uri::fileUriFromPath(input);
			test::check(uri.isFileUri(), "isFileUri");
			test::check(uri.hasAuthority(), "hasAuthority"); // fileUriFromPath always sets an empty authority
			test::compare(uri.authority(), "");
			test::compare(uri.fsPath(), expectedAbsolute(input));
		}
		{
			const auto input = (std::filesystem::current_path() / "foo" / "bar").string();
			const auto uri    = Uri::fileUriFromPath(input);
			test::compare(uri.fsPath(), expectedAbsolute(input));
		}
		{
			const auto input = (std::filesystem::current_path() / "foo bar").string();
			const auto uri    = Uri::fileUriFromPath(input);
			test::compare(uri.fsPath(), expectedAbsolute(input));
			test::check(uri.toString().find("%20") != std::string::npos, "spaceIsPercentEncoded");
		}
		{
			const auto uri = Uri::fileUriFromPath("relative/path");
			test::compare(Uri::parse(uri.toString()).fsPath(), uri.fsPath());
		}
	});

	app.addTest("Equality", [](std::string_view lhsInput, std::string_view rhsInput, bool expectedEqual)
	{
		const auto lhs = Uri::parse(lhsInput);
		const auto rhs = Uri::parse(rhsInput);
		test::check(lhs.isValid(), "lhsValid");
		test::check(rhs.isValid(), "rhsValid");

		test::compare(lhs == rhs, expectedEqual);
		test::compare(rhs == lhs, expectedEqual); // symmetry
		test::compare(lhs != rhs, !expectedEqual);
		test::compare(rhs != lhs, !expectedEqual);
	})({
		{"IdenticalInput", {"http://example.com/path?query#fragment", "http://example.com/path?query#fragment", true}},

		{"SchemeCaseInsensitive", {"HTTP://example.com/path",  "http://example.com/path", true}},
		{"SchemeDiffSameLen",     {"cat://example.com/path",   "dog://example.com/path",  false}},
		{"SchemeDiffLen",         {"http://example.com/path",  "ftp://example.com/path",  false}},

		{"AuthorityIdentical",             {"http://example.com/path", "http://example.com/path", true}},
		{"AuthorityDiffContentSameLen",    {"http://example.com/path", "http://example.org/path", false}},
		{"AuthorityDiffLen",               {"http://example.com/path", "http://example.co/path",  false}},
		{"AuthorityPresenceDiffers",       {"file:/path",              "file:///path",            false}},
		{"AuthorityEscapeCaseInsensitive", {"http://a%2fb/path",       "http://a%2Fb/path",       true}},

		{"PathIdentical",          {"http://example.com/a",  "http://example.com/a", true}},
		{"PathDiffContentSameLen", {"http://example.com/a",  "http://example.com/b", false}},
		{"PathDiffLen",            {"http://example.com/aa", "http://example.com/a", false}},

		{"NoQueryBothAbsent",            {"http://example.com/path",           "http://example.com/path",           true}},
		{"QueryIdentical",               {"http://example.com/path?key=value", "http://example.com/path?key=value", true}},
		{"QueryDiffContentSameLen",      {"http://example.com/path?aaaa",      "http://example.com/path?bbbb",      false}},
		{"QueryDiffLen",                 {"http://example.com/path?a",         "http://example.com/path?ab",        false}},
		{"QueryPresenceDiffersNonEmpty", {"http://example.com/path?query",     "http://example.com/path",           false}},
		{"QueryPresenceDiffersEmpty",    {"file:///path?",                     "file:///path",                      false}},
		{"QueryEscapeCaseInsensitive",   {"file:///path?a%2fb",                "file:///path?a%2Fb",                true}},

		{"NoFragmentBothAbsent",            {"http://example.com/path",         "http://example.com/path",         true}},
		{"FragmentIdentical",               {"http://example.com/path#section", "http://example.com/path#section", true}},
		{"FragmentDiffContentSameLen",      {"http://example.com/path#aaaa",    "http://example.com/path#bbbb",    false}},
		{"FragmentDiffLen",                 {"http://example.com/path#a",       "http://example.com/path#ab",      false}},
		{"FragmentPresenceDiffersNonEmpty", {"http://example.com/path#frag",    "http://example.com/path",         false}},
		{"FragmentPresenceDiffersEmpty",    {"file:///path#",                   "file:///path",                    false}},
		{"FragmentEscapeCaseInsensitive",   {"file:///path#a%2fb",              "file:///path#a%2Fb",              true}},

		{"AuthorityPathBoundaryDiffers", {"http://example.com/x", "http://example.co/m/x", false}},
	});

	app.addTest("LessThan", [](std::string_view lhsInput, std::string_view rhsInput, bool expectedLess)
	{
		const auto lhs = Uri::parse(lhsInput);
		const auto rhs = Uri::parse(rhsInput);
		test::check(lhs.isValid(), "lhsValid");
		test::check(rhs.isValid(), "rhsValid");
		test::compare(lhs < rhs, expectedLess);
	})({
		{"OrderedByPath",       {"file:///a", "file:///b",  true}},
		{"ReverseNotLess",      {"file:///b", "file:///a",  false}},
		{"EqualNotLess",        {"file:///a", "file:///a",  false}},
		{"OrderedByScheme",     {"a://x/p",   "b://x/p",    true}},
		{"ShorterIsPrefixLess", {"file:///a", "file:///ab", true}},
	});

	app.addTest("EqualityRawDataCollisionAcrossSplit", [](){
		auto splitA = Uri();
		splitA.setScheme("http");
		splitA.setAuthority("example.com");
		splitA.setPath("/x");

		auto splitB = Uri();
		splitB.setScheme("http");
		splitB.setAuthority("example.co");
		splitB.setPath("m/x");

		test::compare(splitA.toString(), "http://example.com/x");
		test::compare(splitB.toString(), "http://example.co/m/x");
		test::check(splitA != splitB, "differentSplitNotEqual");
		test::check(!(splitA < splitB) && !(splitB < splitA), "differentSplitNeitherLessThan");
	});

	app.addTest("Encode", [](std::string_view decoded, std::string_view exclude, std::string_view expectedEncoded)
	{
		test::compare(Uri::encode(decoded, exclude), expectedEncoded);
	})({
		{"Empty",                 {"",             {},   ""            }},
		{"UnreservedUnchanged",   {"abcABC123-._", "",   "abcABC123-._"}},
		{"SpaceEncoded",          {"a b",          {},   "a%20b"       }},
		{"SlashEncoded",          {"a/b",          {},   "a%2Fb"       }},
		{"ExcludeKeepsCharRaw",   {"a/b",          "/",  "a/b"         }},
		{"MultipleExcludedChars", {"a/b?c",        "/?", "a/b?c"       }},
	});

	app.addTest("Decode", [](std::string_view encoded, std::string_view expectedDecoded)
	{
		test::compare(Uri::decode(encoded), expectedDecoded);
	})({
		{"Empty",                   {"",      ""   }},
		{"NoEscapes",               {"abc",   "abc"}},
		{"LowercaseHexEscape",      {"%2f",   "/"  }},
		{"UppercaseHexEscape",      {"%2F",   "/"  }},
		{"SpaceEscape",             {"a%20b", "a b"}},
		{"TruncatedTrailingEscape", {"a%2",   "a%2"}},
		{"LoneTrailingPercent",     {"a%",    "a%" }},
		{"InvalidHexDigits",        {"a%zzb", ""   }},
	});

	return app.main(argc, argv);
}
