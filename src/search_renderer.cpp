/*
 * Copyright 2011 Emmanuel Engelhart <kelson@kiwix.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU  General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */


#include <cmath>

#include "search_renderer.h"
#include "searcher.h"
#include "reader.h"
#include "library.h"
#include "name_mapper.h"

#include "tools/archiveTools.h"

#include <zim/search.h>

#include <mustache.hpp>
#include "kiwixlib-resources.h"
#include "tools/stringTools.h"

namespace kiwix
{

/* Constructor */
SearchRenderer::SearchRenderer(Searcher* searcher, NameMapper* mapper)
    : SearchRenderer(
        searcher->getSearchResultSet(),
        mapper,
        nullptr,
        searcher->getEstimatedResultCount(),
        searcher->getResultStart())
{}

SearchRenderer::SearchRenderer(zim::SearchResultSet srs, NameMapper* mapper,
                      unsigned int start, unsigned int estimatedResultCount)
    : SearchRenderer(srs, mapper, nullptr, start, estimatedResultCount)
{}

SearchRenderer::SearchRenderer(zim::SearchResultSet srs, NameMapper* mapper, Library* library,
                      unsigned int start, unsigned int estimatedResultCount)
    : m_srs(srs),
      mp_nameMapper(mapper),
      mp_library(library),
      protocolPrefix("zim://"),
      searchProtocolPrefix("search://"),
      estimatedResultCount(estimatedResultCount),
      resultStart(start)
{}

/* Destructor */
SearchRenderer::~SearchRenderer() = default;

void SearchRenderer::setSearchPattern(const std::string& pattern)
{
  searchPattern = pattern;
}

void SearchRenderer::setSearchBookNames(const std::set<std::string>& bookNames)
{
  searchBookNames = bookNames;
}

void SearchRenderer::setProtocolPrefix(const std::string& prefix)
{
  this->protocolPrefix = prefix;
}

void SearchRenderer::setSearchProtocolPrefix(const std::string& prefix)
{
  this->searchProtocolPrefix = prefix;
}

kainjow::mustache::data buildQueryData
(
  const std::string& pattern,
  const std::set<std::string>& bookNames
) {
  kainjow::mustache::data query;
  query.set("pattern", kiwix::encodeDiples(pattern));
  std::ostringstream ss;
  ss << "?pattern=" << urlEncode(pattern);
  for (auto& bookName: bookNames) {
    ss << "&content="<<urlEncode(bookName);
  }
  query.set("path", ss.str());
  return query;
}


kainjow::mustache::data buildPagination(
  unsigned int pageLength,
  unsigned int resultsCount,
  unsigned int resultsStart
)
{
  assert(pageLength!=0);
  assert(resultsCount!=0);
  kainjow::mustache::data pagination;

  // First we want to display pages starting at a multiple of `pageLength`
  // so, let's calculate the start index of the current page.
  auto currentPage = resultsStart/pageLength;
  auto lastPage = ((resultsCount-1)/pageLength);
  auto lastPageStart = lastPage*pageLength;
  auto nbPages = lastPage + 1;

  auto firstPageGenerated = currentPage > 4 ? currentPage-4 : 0;
  auto lastPageGenerated = min(currentPage+4, lastPage);

  kainjow::mustache::data pages{kainjow::mustache::data::type::list};
  for (auto i=firstPageGenerated; i<=lastPageGenerated; i++) {
    kainjow::mustache::data page;
    page.set("label", to_string(i+1));
    page.set("start", to_string(i*pageLength));
    if (i == currentPage) {
      page.set("current", true);
    }
    // First and last pages are special pages.
    if (i == 0) {
      pagination.set("firstPage", page);
    } else if (i==lastPage) {
      pagination.set("lastPage", page);
    } else {
      pages.push_back(page);
    }
  }

  if (firstPageGenerated!=0) {
    kainjow::mustache::data page;
    page.set("label", to_string(0));
    page.set("start", to_string(0));
    pagination.set("firstPage", page);
  }

  if (lastPageGenerated!=lastPage) {
    kainjow::mustache::data page;
    page.set("label", to_string(nbPages));
    page.set("start", to_string(lastPageStart));
    pagination.set("lastPage", page);
  }

  pagination.set("itemsPerPage", to_string(pageLength));
  pagination.set("hasPages", nbPages!=1);
  pagination.set("pages", pages);
  return pagination;
}

std::string SearchRenderer::getHtml()
{
  // Build the results list
  kainjow::mustache::data results;
  kainjow::mustache::data items{kainjow::mustache::data::type::list};
  for (auto it = m_srs.begin(); it != m_srs.end(); it++) {
    kainjow::mustache::data result;
    std::string zim_id(it.getZimId());
    result.set("title", it.getTitle());
    result.set("absolutePath", urlEncode(mp_nameMapper->getNameForId(zim_id)+"/"+it.getPath()));
    result.set("snippet", it.getSnippet());
    if (mp_library) {
      result.set("bookTitle", mp_library->getBookById(zim_id).getTitle());
    }
    if (it.getWordCount() >= 0) {
      result.set("wordCount", kiwix::beautifyInteger(it.getWordCount()));
    }

    items.push_back(result);
  }
  results.set("items", items);
  results.set("count", kiwix::beautifyInteger(estimatedResultCount));
  results.set("hasResults", estimatedResultCount != 0);
  results.set("start", kiwix::beautifyInteger(resultStart+1));
  results.set("end", kiwix::beautifyInteger(min(resultStart+pageLength, estimatedResultCount)));

  // pagination
  auto pagination = buildPagination(
    pageLength,
    estimatedResultCount,
    resultStart
  );

  kainjow::mustache::data query = buildQueryData(
    searchPattern,
    searchBookNames
  );

  std::string template_str = RESOURCE::templates::search_result_html;
  kainjow::mustache::mustache tmpl(template_str);

  kainjow::mustache::data allData;
  allData.set("results", results);
  allData.set("protocolPrefix", this->protocolPrefix);
  allData.set("searchProtocolPrefix", this->searchProtocolPrefix);
  allData.set("pagination", pagination);
  allData.set("query", query);

  std::stringstream ss;
  tmpl.render(allData, [&ss](const std::string& str) { ss << str; });
  if (!tmpl.is_valid()) {
    throw std::runtime_error(tmpl.error_message());
  }
  return ss.str();
}

}
