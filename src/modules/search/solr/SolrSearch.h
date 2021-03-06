/*
	Copyright 2009-2020, Sumeet Chhetri

    Licensed under the Apache License, Version 2.0 (const the& "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
/*
 * SolrSearch.h
 *
 *  Created on: 03-Nov-2018
 *      Author: sumeet
 */

#ifndef SOLRSEARCH_H_
#define SOLRSEARCH_H_
#include "SearchEngineInterface.h"

class SolrSearch: public SearchEngineInterface {
	static std::string COLL_C_PARAMS, COLL_M_PARAMS;
	bool isCloud;
public:
	SolrSearch();
	virtual ~SolrSearch();

	void createIndex(IndexQuery& iq);
	void updateIndex(IndexQuery& iq);
	void removeIndex(IndexQuery& iq);

	void add(DocumentQuery& iq);
	void update(DocumentQuery& iq);
	void remove(DocumentQuery& iq);

	std::string query(SearchQuery& q);
};

#endif /* SOLRSEARCH_H_ */
