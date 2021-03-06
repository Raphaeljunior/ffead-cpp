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
#include "LibpqDataSourceImpl.h"

std::atomic<bool> LibpqDataSourceImpl::done = false;

DSType LibpqDataSourceImpl::getType() {
	return SD_RAW_SQLPG;
}

LibpqDataSourceImpl::LibpqDataSourceImpl(const std::string& url, bool isAsync) {
	this->url = url;
#ifdef HAVE_LIBPQ
	conn = NULL;
#endif
	trx = false;
	this->isAsync = isAsync;
	fd = -1;
}

LibpqDataSourceImpl::~LibpqDataSourceImpl() {
#ifdef HAVE_LIBPQ
	done = true;
	Thread::sSleep(3);
	if(conn!=NULL) {
		PQfinish(conn);
	}
#endif
}

bool LibpqDataSourceImpl::init() {
#ifdef HAVE_LIBPQ
	if(conn!=NULL) return true;
	conn = PQconnectdb(url.c_str());
	if (PQstatus(conn) == CONNECTION_BAD) {
		fprintf(stderr, "Connection to database failed: %s\n", PQerrorMessage(conn));
		PQfinish(conn);
		conn = NULL;
		return false;
	}
	fd = PQsocket(conn);
	if(isAsync) {
		PQsetnonblocking(conn, 1);
		if(RequestReaderHandler::getInstance()!=NULL) {
			rdTsk = new PgReadTask(this);
			RequestReaderHandler::getInstance()->addSf(this);
		} else {
			Thread* pthread = new Thread(&handle, this);
			pthread->execute();
		}
	}
#endif
	return true;
}

void* LibpqDataSourceImpl::beginAsync(void* vitem) {
	if(isAsync) {
		int status = -1;
		std::vector<LibpqParam> pvals;
		__AsyncReq* areq = NULL;
#ifdef HAVE_LIBPQ
		executeQueryInt("BEGIN", pvals, true, status, NULL, NULL, NULL, vitem, false, &areq);
#endif
		return areq;
	}
	return NULL;
}

void* LibpqDataSourceImpl::commitAsync(void* vitem) {
	if(isAsync) {
		int status = -1;
		std::vector<LibpqParam> pvals;
		__AsyncReq* areq = NULL;
#ifdef HAVE_LIBPQ
		executeQueryInt("COMMIT", pvals, true, status, NULL, NULL, NULL, vitem, false, &areq);
#endif
		return areq;
	}
	return NULL;
}

void* LibpqDataSourceImpl::rollbackAsync(void* vitem) {
	if(isAsync) {
		int status = -1;
		std::vector<LibpqParam> pvals;
		__AsyncReq* areq = NULL;
#ifdef HAVE_LIBPQ
		executeQueryInt("ROLLBACK", pvals, true, status, NULL, NULL, NULL, vitem, false, &areq);
#endif
		return areq;
	}
	return NULL;
}

bool LibpqDataSourceImpl::begin() {
#ifdef HAVE_LIBPQ
	if(isAsync) {
		throw std::runtime_error("Please call beginAsync");
	}
	PGresult *res = PQexec(conn, "BEGIN");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("BEGIN command failed\n");
		PQclear(res);
		//PQfinish(conn);
		return false;
	}
	PQclear(res);
	trx = true;
#endif
	return true;
}

bool LibpqDataSourceImpl::commit() {
#ifdef HAVE_LIBPQ
	if(isAsync) {
		throw std::runtime_error("Please call commitAsync");
	}
	PGresult *res = PQexec(conn, "COMMIT");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("COMMIT command failed\n");
		PQclear(res);
		//PQfinish(conn);
		return false;
	}
	PQclear(res);
	trx = false;
#endif
	return true;
}

bool LibpqDataSourceImpl::rollback() {
#ifdef HAVE_LIBPQ
	if(isAsync) {
		throw std::runtime_error("Please call rollbackAsync");
	}
	PGresult *res = PQexec(conn, "ROLLBACK");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("ROLLBACK command failed\n");
		PQclear(res);
		//PQfinish(conn);
		return false;
	}
	PQclear(res);
	trx = false;
#endif
	return true;
}

void LibpqDataSourceImpl::ADD_INT2(std::vector<LibpqParam>& pvals, unsigned short i, bool isH) {
	pvals.push_back({.p = NULL, .s = (isH?htons(i):i), .i = 0, .li = 0, .l = 2, .t = 1, .b = 1});
}

void LibpqDataSourceImpl::ADD_INT4(std::vector<LibpqParam>& pvals, unsigned int i, bool isH) {
	pvals.push_back({.p = NULL, .s = 0, .i = (isH?htonl(i):i), .li = 0, .l = 4, .t = 2, .b = 1});
}

void LibpqDataSourceImpl::ADD_INT8(std::vector<LibpqParam>& pvals, long long i) {
	pvals.push_back({.p = NULL, .s = 0, .i = 0, .li = i, .l = 8, .t = 3, .b = 1});
}

void LibpqDataSourceImpl::ADD_STR(std::vector<LibpqParam>& pvals, const char *i) {
	pvals.push_back({.p = i, .s = 0, .i = 0, .li = 0, .l = strlen(i), .t = 4, .b = 0});
}

void LibpqDataSourceImpl::ADD_BIN(std::vector<LibpqParam>& pvals, const char *i, int len) {
	pvals.push_back({.p = i, .s = 0, .i = 0, .li = 0, .l = (size_t)len, .t = 5, .b = 1});
}

__AsyncReq* LibpqDataSourceImpl::getNext() {
	__AsyncReq* ar = NULL;
	if(rdTsk==NULL)c_mutex.lock();
	if(Q.size()>0) {
		ar = Q.front();
		Q.pop();
	}
	if(rdTsk==NULL)c_mutex.unlock();
	return ar;
}

void* LibpqDataSourceImpl::handle(void* inp) {
	LibpqDataSourceImpl* ths = (LibpqDataSourceImpl*)inp;
#ifdef HAVE_LIBPQ
	struct timeval tv;
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	while(!done) {
		ths->c_mutex.lock();
		while (!ths->cvar)
		ths->c_mutex.conditionalWait();
		ths->c_mutex.unlock();

		ths->cvar = false;

		__AsyncReq* item = NULL;
		while((item = ths->getNext())!=NULL && !done) {
			int counter = -1;
			while(item->q.size()>0) {
				counter ++;
				__AsynQuery* q = item->q.front();
				item->q.pop();

				int psize = (int)q->pvals.size();
				const char *paramValues[psize];
				int paramLengths[psize];
				int paramBinary[psize];
				for (int var = 0; var < psize; ++var) {
					if(q->pvals.at(var).t==1) {//short
						paramValues[var] = (char *)&q->pvals.at(var).s;
						paramLengths[var] = q->pvals.at(var).l;
					} else if(q->pvals.at(var).t==2) {//int
						paramValues[var] = (char *)&q->pvals.at(var).i;
						paramLengths[var] = q->pvals.at(var).l;
					} else if(q->pvals.at(var).t==3) {//long
						paramValues[var] = (char *)&q->pvals.at(var).li;
						paramLengths[var] = q->pvals.at(var).l;
					} else {
						paramValues[var] = q->pvals.at(var).p;
						paramLengths[var] = q->pvals.at(var).l;
					}
					paramBinary[var] = q->pvals.at(var).b?1:0;
				}

				std::string stmtName;
				if(q->isPrepared) {
					if(ths->prepStmtMap.find(q->query)==ths->prepStmtMap.end()) {
						stmtName = CastUtil::fromNumber(ths->prepStmtMap.size()+1);
						ths->prepStmtMap[q->query] = stmtName;
						PGresult* res = PQprepare(ths->conn, stmtName.c_str(), q->query.c_str(), psize, NULL);
						if (PQresultStatus(res) != PGRES_COMMAND_OK) {
							fprintf(stderr, "PREPARE failed: %s", PQerrorMessage(ths->conn));
							PQclear(res);
							if(q->cmcb!=NULL) {
								q->cmcb(q->ctx, false, q->query, counter);
							} else if(item->cmcb!=NULL) {
								item->cmcb(item->ctx, false, q->query, counter);
							}
							delete q;
							while(item->q.size()>0) {
								__AsynQuery* qr = item->q.front();
								item->q.pop();
								delete qr;
							}
							break;
						}
					} else {
						stmtName = ths->prepStmtMap[q->query];
					}
				}

				int qs = -1;
				if(q->isPrepared) {
					qs = PQsendQueryPrepared(ths->conn, stmtName.c_str(), psize, paramValues, paramLengths, paramBinary, 1);
				} else {
					qs = PQsendQueryParams(ths->conn, q->query.c_str(), psize, NULL, paramValues, paramLengths, paramBinary, 1);
				}

				if (!qs) {
					fprintf(stderr, "Failed to send query %s\n", PQerrorMessage(ths->conn));
					if(q->cmcb!=NULL) {
						q->cmcb(q->ctx, false, q->query, counter);
					} else if(item->cmcb!=NULL) {
						item->cmcb(item->ctx, false, q->query, counter);
					}
					delete q;
					while(item->q.size()>0) {
						__AsynQuery* qr = item->q.front();
						item->q.pop();
						delete qr;
					}
					break;
				}

				bool resDone = false;
				while(!resDone) {
					fd_set read_fds;
					FD_ZERO(&read_fds);
					FD_SET(ths->fd, &read_fds);

					int retval = select(ths->fd + 1, &read_fds, NULL, NULL, &tv);
					switch (retval) {
						case -1:
							perror("select() failed");
							resDone = true;
							break;
						case 0:
							break;
						default:
							if (FD_ISSET(ths->fd, &read_fds)) {
								if (!PQconsumeInput(ths->conn)) {
									fprintf(stderr, "Failed to consume pg input: %s\n", PQerrorMessage(ths->conn));
									throw std::runtime_error("Invalid connection state");
								}
								if(PQisBusy(ths->conn)==1) {
									continue;
								}

								PGresult* res = NULL;
								bool itemDone = false;
								while ((res = PQgetResult(ths->conn))!=NULL && !done) {
									if(itemDone) {
										PQclear(res);
										continue;
									}
									if(q->isSelect) {
										if (PQresultStatus(res) != PGRES_TUPLES_OK) {
											if(q->cmcb!=NULL) {
												q->cmcb(q->ctx, false, q->query, counter);
											} else if(item->cmcb!=NULL) {
												item->cmcb(item->ctx, false, q->query, counter);
											}
											fprintf(stderr, "SELECT failed: %s", PQerrorMessage(ths->conn));
											PQclear(res);
										} else if(q->cb!=NULL) {
											int cols = PQnfields(res);
											int rows = PQntuples(res);
											std::vector<LibpqRes> row;
											for(int i=0; i<rows; i++) {
												row.clear();
												for (int j = 0; j < cols; ++j) {
													row.push_back({.n = PQfname(res, j), .d = PQgetvalue(res, i, j), .l = PQgetlength(res, i, j)});
												}
												q->cb(q->ctx, i, row);
											}
										}
									} else {
										if (PQresultStatus(res) != PGRES_COMMAND_OK) {
											if(q->cmcb!=NULL) {
												q->cmcb(q->ctx, false, q->query, counter);
											} else if(item->cmcb!=NULL) {
												item->cmcb(item->ctx, false, q->query, counter);
											}
											fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(ths->conn));
											PQclear(res);
										}
									}

									if(item->cnt--==-1) {
										if(q->cmcb!=NULL) {
											q->cmcb(q->ctx, true, q->query, counter);
										} else if(item->cmcb!=NULL) {
											item->cmcb(item->ctx, true, q->query, counter);
										}
										itemDone = true;
									}
									PQclear(res);
								}
								resDone = true;
							}
					}
				}
				delete q;
			}
			delete item;
		}
	}
#endif
	return NULL;
}

void PgReadTask::run() {
#ifdef HAVE_LIBPQ
	LibpqDataSourceImpl* ths = (LibpqDataSourceImpl*)sif;
	if (!PQconsumeInput(ths->conn)) {
		fprintf(stderr, "Failed to consume pg input: %s\n", PQerrorMessage(ths->conn));
		throw std::runtime_error("Invalid connection state");
	}
	//if(PQisBusy(ths->conn)==1) {
	//	continue;
	//}

	PGresult* res = NULL;
	bool ritemDone = false;
	while ((res = PQgetResult(ths->conn))!=NULL) {
		if(ritemDone) {
			PQclear(res);
			continue;
		}
		if(hasPrepare) {
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				fprintf(stderr, "PREPARE failed: %s", PQerrorMessage(ths->conn));
				PQclear(res);
				if(q->cmcb!=NULL) {
					q->cmcb(q->ctx, false, q->query, counter);
				} else if(ritem->cmcb!=NULL) {
					ritem->cmcb(ritem->ctx, false, q->query, counter);
				}
				while(ritem->q.size()>0) {
					__AsynQuery* qr = ritem->q.front();
					ritem->q.pop();
					delete qr;
				}
				delete q;
				q = NULL;
				ritemDone = true;
			} else {
				//fprintf(stdout, "PREPARE response....\n");fflush(stdout);
			}
		} else if(q->isSelect) {
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				if(q->cmcb!=NULL) {
					q->cmcb(q->ctx, false, q->query, counter);
				} else if(ritem->cmcb!=NULL) {
					ritem->cmcb(ritem->ctx, false, q->query, counter);
				}
				fprintf(stderr, "SELECT failed: %s", PQerrorMessage(ths->conn));
				PQclear(res);
				while(ritem->q.size()>0) {
					__AsynQuery* qr = ritem->q.front();
					ritem->q.pop();
					delete qr;
				}
				ritemDone = true;
			} else if(q->cb!=NULL) {
				//fprintf(stdout, "SELECT response\n");fflush(stdout);
				int cols = PQnfields(res);
				int rows = PQntuples(res);
				std::vector<LibpqRes> row;
				for(int i=0; i<rows; i++) {
					row.clear();
					for (int j = 0; j < cols; ++j) {
						row.push_back({.n = PQfname(res, j), .d = PQgetvalue(res, i, j), .l = PQgetlength(res, i, j)});
					}
					q->cb(q->ctx, i, row);
				}
				if(ritem->cnt--==-1) {
					if(q->cmcb!=NULL) {
						q->cmcb(q->ctx, true, q->query, counter);
					} else if(ritem->cmcb!=NULL) {
						ritem->cmcb(ritem->ctx, true, q->query, counter);
					}
					ritemDone = true;
				}
			}
			delete q;
			q = NULL;
		} else {
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				if(q->cmcb!=NULL) {
					q->cmcb(q->ctx, false, q->query, counter);
				} else if(ritem->cmcb!=NULL) {
					ritem->cmcb(ritem->ctx, false, q->query, counter);
				}
				fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(ths->conn));
				PQclear(res);
				while(ritem->q.size()>0) {
					__AsynQuery* qr = ritem->q.front();
					ritem->q.pop();
					delete qr;
				}
				ritemDone = true;
			} else {
				//fprintf(stdout, "UPDATE response\n");fflush(stdout);
			}
			if(ritem->cnt--==-1) {
				if(q->cmcb!=NULL) {
					q->cmcb(q->ctx, true, q->query, counter);
				} else if(ritem->cmcb!=NULL) {
					ritem->cmcb(ritem->ctx, true, q->query, counter);
				}
				ritemDone = true;
			}
			delete q;
			q = NULL;
		}
		PQclear(res);
	}
	flux = false;
	submit(NULL);
	if(hasPrepare) hasPrepare = false;
#endif
}

void PgReadTask::submit(__AsyncReq* nitem) {
	LibpqDataSourceImpl* ths = (LibpqDataSourceImpl*)sif;
#ifdef HAVE_LIBPQ
	if(flux && nitem!=NULL) {
		ths->Q.push(nitem);
		//fprintf(stdout, "Add query to Q....\n");fflush(stdout);
		return;
	}

	if(ritem!=NULL && ritem->cnt==-2 && !hasPrepare) {
		delete ritem;
		ritem = NULL;
		counter = -1;
		//fprintf(stdout, "Clear query....\n");fflush(stdout);
	}

	if(nitem==NULL && ritem==NULL) {
		ritem = ths->getNext();
		//fprintf(stdout, "New query from Q....\n");fflush(stdout);
	} else if(ritem==NULL && nitem!=NULL) {
		ritem = nitem;
		//fprintf(stdout, "New query....\n");fflush(stdout);
	}

	if(ritem!=NULL) {
		if((hasPrepare && q!=NULL) || ritem->q.size()>0) {
			if(!hasPrepare) {
				counter ++;
				q = ritem->q.front();
				ritem->q.pop();
			}

			int psize = (int)q->pvals.size();

			if(q->isPrepared) {
				if(ths->prepStmtMap.find(q->query)==ths->prepStmtMap.end()) {
					//fprintf(stdout, "Prepare query....\n");fflush(stdout);
					hasPrepare = true;
					q->stmtName = CastUtil::fromNumber(ths->prepStmtMap.size()+1);
					ths->prepStmtMap[q->query] = q->stmtName;
					int qs = PQsendPrepare(ths->conn, q->stmtName.c_str(), q->query.c_str(), psize, NULL);

					if (!qs) {
						fprintf(stderr, "Failed to prepare query %s\n", PQerrorMessage(ths->conn));
						if(q->cmcb!=NULL) {
							q->cmcb(q->ctx, false, q->query, counter);
						} else if(ritem->cmcb!=NULL) {
							ritem->cmcb(ritem->ctx, false, q->query, counter);
						}
						delete q;
						while(ritem->q.size()>0) {
							__AsynQuery* qr = ritem->q.front();
							ritem->q.pop();
							delete qr;
						}
						delete ritem;
						ritem = NULL;
					} else {
						flux = true;
						PQflush(ths->conn);
					}
					return;
				} else {
					q->stmtName = ths->prepStmtMap[q->query];
				}
			}

			const char *paramValues[psize];
			int paramLengths[psize];
			int paramBinary[psize];
			for (int var = 0; var < psize; ++var) {
				if(q->pvals.at(var).t==1) {//short
					paramValues[var] = (char *)&q->pvals.at(var).s;
					paramLengths[var] = q->pvals.at(var).l;
				} else if(q->pvals.at(var).t==2) {//int
					paramValues[var] = (char *)&q->pvals.at(var).i;
					paramLengths[var] = q->pvals.at(var).l;
				} else if(q->pvals.at(var).t==3) {//long
					paramValues[var] = (char *)&q->pvals.at(var).li;
					paramLengths[var] = q->pvals.at(var).l;
				} else {
					paramValues[var] = q->pvals.at(var).p;
					paramLengths[var] = q->pvals.at(var).l;
				}
				paramBinary[var] = q->pvals.at(var).b?1:0;
			}

			int qs = -1;
			if(q->isPrepared) {
				qs = PQsendQueryPrepared(ths->conn, q->stmtName.c_str(), psize, paramValues, paramLengths, paramBinary, 1);
			} else {
				qs = PQsendQueryParams(ths->conn, q->query.c_str(), psize, NULL, paramValues, paramLengths, paramBinary, 1);
			}
			//fprintf(stdout, "Send query....\n");fflush(stdout);

			if (!qs) {
				fprintf(stderr, "Failed to send query %s\n", PQerrorMessage(ths->conn));
				if(q->cmcb!=NULL) {
					q->cmcb(q->ctx, false, q->query, counter);
				} else if(ritem->cmcb!=NULL) {
					ritem->cmcb(ritem->ctx, false, q->query, counter);
				}
				delete q;
				while(ritem->q.size()>0) {
					__AsynQuery* qr = ritem->q.front();
					ritem->q.pop();
					delete qr;
				}
				delete ritem;
				ritem = NULL;
			} else {
				flux = true;
				PQflush(ths->conn);
			}
		}
	} else if(nitem!=NULL) {
		//ths->c_mutex.lock();
		ths->Q.push(nitem);
		//ths->c_mutex.unlock();
		//fprintf(stdout, "Add query to Q....\n");fflush(stdout);
	}
#endif
}

void LibpqDataSourceImpl::completeAsync(void* vitem, void* ctx, LipqComplFunc cmcb) {
	__AsyncReq* item = (__AsyncReq*)vitem;
	if(item!=NULL) {
		item->cmcb = cmcb;
		item->ctx = ctx;

		if(rdTsk!=NULL) {
			((PgReadTask*)rdTsk)->submit(item);
			return;
		}

		c_mutex.lock();
		Q.push(item);
		cvar = true;
		c_mutex.conditionalNotifyOne();
		c_mutex.unlock();
	}
}

#ifdef HAVE_LIBPQ
PGresult* LibpqDataSourceImpl::executeQueryInt(const std::string &query, const std::vector<LibpqParam>& pvals, bool isPrepared, int& status,
		void* ctx, LipqResFunc cb, LipqComplFunc cmcb, void* vitem, bool isSelect, __AsyncReq** areq) {
	if(isAsync) {
		status = 1;
		__AsyncReq* item = vitem!=NULL?(__AsyncReq*)vitem:new __AsyncReq;
		if(vitem==NULL) {
			item->cnt = -1;
		} else {
			item->cnt++;
		}

		__AsynQuery* q = new __AsynQuery;
		q->query = query;
		q->isPrepared = isPrepared;
		q->pvals = pvals;
		q->ctx = ctx;
		q->cb = cb;
		q->cmcb = cmcb;
		q->isSelect = isSelect;
		item->q.push(q);

		*areq = item;

		return NULL;
	} else {
		int psize = (int)pvals.size();
		const char *paramValues[psize];
		int paramLengths[psize];
		int paramBinary[psize];
		for (int var = 0; var < psize; ++var) {
			if(pvals.at(var).t==1) {//short
				paramValues[var] = (char *)&pvals.at(var).s;
				paramLengths[var] = pvals.at(var).l;
			} else if(pvals.at(var).t==2) {//int
				paramValues[var] = (char *)&pvals.at(var).i;
				paramLengths[var] = pvals.at(var).l;
			} else if(pvals.at(var).t==3) {//long
				paramValues[var] = (char *)&pvals.at(var).li;
				paramLengths[var] = pvals.at(var).l;
			} else {
				paramValues[var] = pvals.at(var).p;
				paramLengths[var] = pvals.at(var).l;
			}
			paramBinary[var] = pvals.at(var).b?1:0;
		}
		std::string stmtName;
		if(isPrepared) {
			if(prepStmtMap.find(query)==prepStmtMap.end()) {
				stmtName = CastUtil::fromNumber(prepStmtMap.size()+1);
				prepStmtMap[query] = stmtName;
				PGresult* res = PQprepare(conn, stmtName.c_str(), query.c_str(), psize, NULL);
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					fprintf(stderr, "PREPARE failed: %s", PQerrorMessage(conn));
					PQclear(res);
					return NULL;
				}
			} else {
				stmtName = prepStmtMap[query];
			}
		}
		if(isPrepared) {
			return PQexecPrepared(conn, stmtName.c_str(), psize, paramValues, paramLengths, paramBinary, 1);
		} else if (psize>0) {
			return PQexecParams(conn, query.c_str(), psize, NULL, paramValues, paramLengths, paramBinary, 1);
		} else {
			int qs = PQsendQuery(conn, query.c_str());
			if (!qs) {
				fprintf(stderr, "Failed to send query %s\n", PQerrorMessage(conn));
			}
			return NULL;
		}
	}
	return NULL;
}
#endif

void* LibpqDataSourceImpl::executeUpdateQueryAsync(const std::string &query, const std::vector<LibpqParam>& pvals, void* ctx, LipqComplFunc cmcb, void* vitem, bool isPrepared) {
	int status = -1;
	__AsyncReq* areq = NULL;
#ifdef HAVE_LIBPQ
	executeQueryInt(query, pvals, isPrepared, status, ctx, NULL, cmcb, vitem, false, &areq);
#endif
	return areq;
}

void* LibpqDataSourceImpl::executeQueryAsync(const std::string &query, const std::vector<LibpqParam>& pvals, void* ctx, LipqResFunc cb, LipqComplFunc cmcb, void* vitem, bool isPrepared) {
	int status = -1;
	__AsyncReq* areq = NULL;
#ifdef HAVE_LIBPQ
	executeQueryInt(query, pvals, isPrepared, status, ctx, cb, cmcb, vitem, true, &areq);
#endif
	return areq;
}

void LibpqDataSourceImpl::executeMultiQuery(const std::string &query, void* ctx, LipqResFunc cb, LipqComplFunc cmcb) {
	if(isAsync) {
		throw std::runtime_error("Please call executeQueryAsync");
	}
#ifdef HAVE_LIBPQ
	int status = -1;
	__AsyncReq* areq = NULL;
	std::vector<LibpqParam> pvals;
	executeQueryInt(query, pvals, false, status, NULL, NULL, NULL, NULL, true, &areq);
	PGresult* res = NULL;
	bool stat = true;
	int counter = -1;
	while((res = PQgetResult(conn))!=NULL) {
		counter++;
		if (PQresultStatus(res) != PGRES_TUPLES_OK) {
			fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
			if(cmcb!=NULL) {
				cmcb(ctx, false, query, counter);
			}
			stat = false;
		} else {
			int cols = PQnfields(res);
			int rows = PQntuples(res);
			std::vector<LibpqRes> row;
			for(int i=0; i<rows; i++) {
				row.clear();
				for (int j = 0; j < cols; ++j) {
					row.push_back({.n = PQfname(res, j), .d = PQgetvalue(res, i, j), .l = PQgetlength(res, i, j)});
				}
				cb(ctx, i, row);
			}
		}
		PQclear(res);
	}
	if(stat) {
		if(cmcb!=NULL) {
			cmcb(ctx, stat, query, counter);
		}
	}
#endif
}

void LibpqDataSourceImpl::executeUpdateMultiQuery(const std::string &query, void* ctx, LipqComplFunc cmcb) {
	if(isAsync) {
		throw std::runtime_error("Please call executeQueryAsync");
	}
#ifdef HAVE_LIBPQ
	int status = -1;
	__AsyncReq* areq = NULL;
	std::vector<LibpqParam> pvals;
	executeQueryInt(query, pvals, false, status, NULL, NULL, NULL, NULL, true, &areq);
	PGresult* res = NULL;
	int counter = -1;
	bool stat = true;
	while((res = PQgetResult(conn))!=NULL) {
		counter++;
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(conn));
			PQclear(res);
			stat = false;
			if(cmcb!=NULL) {
				cmcb(ctx, false, query, counter);
			}
		}
		PQclear(res);
	}
	if(stat) {
		if(cmcb!=NULL) {
			cmcb(ctx, stat, query, counter);
		}
	}
#endif
}

void LibpqDataSourceImpl::executeQuery(const std::string &query, const std::vector<LibpqParam>& pvals, void* ctx, LipqResFunc cb, bool isPrepared) {
	if(isAsync) {
		throw std::runtime_error("Please call executeQueryAsync");
	}
#ifdef HAVE_LIBPQ
	int status = -1;
	__AsyncReq* areq = NULL;
	PGresult *res = executeQueryInt(query, pvals, isPrepared, status, NULL, NULL, NULL, NULL, true, &areq);
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
		PQclear(res);
		return;
	}

	int cols = PQnfields(res);
	int rows = PQntuples(res);
	std::vector<LibpqRes> row;
	for(int i=0; i<rows; i++) {
		row.clear();
		for (int j = 0; j < cols; ++j) {
			row.push_back({.n = PQfname(res, j), .d = PQgetvalue(res, i, j), .l = PQgetlength(res, i, j)});
		}
		cb(ctx, i, row);
	}
	PQclear(res);
#endif
}

bool LibpqDataSourceImpl::executeUpdateQuery(const std::string &query, const std::vector<LibpqParam>& pvals, bool isPrepared) {
	if(isAsync) {
		throw std::runtime_error("Please call executeUpdateQueryAsync");
	}
#ifdef HAVE_LIBPQ
	int status = -1;
	__AsyncReq* areq = NULL;
	PGresult *res = executeQueryInt(query, pvals, isPrepared, status, NULL, NULL, NULL, NULL, false, &areq);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		fprintf(stderr, "UPDATE failed: %s", PQerrorMessage(conn));
		PQclear(res);
		return false;
	}
	PQclear(res);
#endif
	return true;
}

PgReadTask::~PgReadTask() {
}

PgReadTask::PgReadTask(SocketInterface* sif) {
	this->sif = sif;
	ritem = NULL;
	counter = -1;
	hasPrepare = false;
	q = NULL;
	flux = false;
}
