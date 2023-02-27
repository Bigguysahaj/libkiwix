#include "html_dumper.h"
#include "libkiwix-resources.h"
#include "tools/otherTools.h"
#include "tools.h"
#include "server/i18n.h"

namespace kiwix
{

/* Constructor */
HTMLDumper::HTMLDumper(const Library* const library, const NameMapper* const nameMapper)
  : LibraryDumper(library, nameMapper)
{
}
/* Destructor */
HTMLDumper::~HTMLDumper()
{
}

kainjow::mustache::list getTagList(std::string tags)
{
  const auto tagsList = kiwix::split(tags, ";", true, false);
  kainjow::mustache::list finalTagList;
  for (auto tag : tagsList) {
  if (tag[0] != '_')
    finalTagList.push_back(kainjow::mustache::object{
      {"tag", tag}
    });
  }
  return finalTagList;
}

std::string HTMLDumper::dumpPlainHTML(kiwix::Filter filter) const
{
  kainjow::mustache::list booksData;
  const auto filteredBooks = library->filter(filter);
  const auto searchQuery = filter.getQuery();
  auto languages = getLanguageList();
  auto categories = getCategoryList();

  for (auto &category : categories) {
    if (category.get("name")->string_value() == filter.getCategory()) {
      category["selected"] = true;
    }
  }

  for (auto &language : languages) {
    if (language.get("lang_code")->string_value() == filter.getLang()) {
      language["selected"] = true;
    }
  }

  for ( const auto& bookId : filteredBooks ) {
    const auto bookObj = library->getBookById(bookId);
    const auto bookTitle = bookObj.getTitle();
    std::string contentId = "";
    try {
      contentId = nameMapper->getNameForId(bookId);
    } catch (...) {}
    const auto bookDescription = bookObj.getDescription();
    const auto langCode = bookObj.getCommaSeparatedLanguages();
    const auto bookIconUrl = rootLocation + "/catalog/v2/illustration/" + bookId +  "/?size=48";
    const auto tags = bookObj.getTags();
    const auto downloadAvailable = (bookObj.getUrl() != "");
    std::string faviconAttr = "style=background-image:url(" + bookIconUrl + ")";
    
    booksData.push_back(kainjow::mustache::object{
      {"id", contentId},
      {"title", bookTitle},
      {"description", bookDescription},
      {"langCode", langCode},
      {"faviconAttr", faviconAttr},
      {"tagList", getTagList(tags)},
      {"downloadAvailable", downloadAvailable}
    });
  }

  auto i18nTranslations = i18n::GetTranslatedStringWithMsgId(m_userLang);

  const auto translations = kainjow::mustache::object{
                    i18nTranslations("search"),
                    i18nTranslations("download"),
                    i18nTranslations("count-of-matching-books", {{"COUNT", to_string(filteredBooks.size())}}),
                    i18nTranslations("book-filtering-all-categories"),
                    i18nTranslations("book-filtering-all-languages"),
                    i18nTranslations("powered-by-kiwix-html"),
                    i18nTranslations("welcome-to-kiwix-server"),
                    i18nTranslations("welcome-page-overzealous-filter", {{"URL", "?lang="}})
  };

  return render_template(
             RESOURCE::templates::no_js_library_page_html,
             kainjow::mustache::object{
               {"root", rootLocation},
               {"books", booksData },
               {"searchQuery", searchQuery},
               {"languages", languages},
               {"categories", categories},
               {"noResults", filteredBooks.size() == 0},
               {"translations", translations}
             }
  );
}

} // namespace kiwix
