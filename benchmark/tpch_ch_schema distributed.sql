-- !/usr/bin/env bash

--  this source code form is subject to the terms of the mozilla public
--  license, v. 2.0.  if a copy of the mpl was not distributed with this
--  file, you can obtain one at http://mozilla.org/mpl/2.0/.

--  copyright 2017-2018 monetdb solutions b.v.

-- sccsid:     @(#)dss.ddl	2.1.8.1
-- Create local tables on each node in the 'insert_bench' database
CREATE TABLE insert_bench.local_nation ON CLUSTER cluster_2S_1R
(
    n_nationkey  INTEGER NOT NULL,
    n_name       CHAR(25) NOT NULL,
    n_regionkey  INTEGER NOT NULL,
    n_comment    VARCHAR(152)
) ENGINE = MergeTree() ORDER BY n_nationkey;

CREATE TABLE insert_bench.local_region ON CLUSTER cluster_2S_1R
(
    r_regionkey  INTEGER NOT NULL,
    r_name       CHAR(25) NOT NULL,
    r_comment    VARCHAR(152)
) ENGINE = MergeTree() ORDER BY r_regionkey;

CREATE TABLE insert_bench.local_part ON CLUSTER cluster_2S_1R
(
    p_partkey     INTEGER NOT NULL,
    p_name        VARCHAR(55) NOT NULL,
    p_mfgr        CHAR(25) NOT NULL,
    p_brand       CHAR(10) NOT NULL,
    p_type        VARCHAR(25) NOT NULL,
    p_size        INTEGER NOT NULL,
    p_container   CHAR(10) NOT NULL,
    p_retailprice DECIMAL(15,2) NOT NULL,
    p_comment     VARCHAR(23) NOT NULL
) ENGINE = MergeTree() ORDER BY p_partkey;

CREATE TABLE insert_bench.local_supplier ON CLUSTER cluster_2S_1R
(
    s_suppkey     INTEGER NOT NULL,
    s_name        CHAR(25) NOT NULL,
    s_address     VARCHAR(40) NOT NULL,
    s_nationkey   INTEGER NOT NULL,
    s_phone       CHAR(15) NOT NULL,
    s_acctbal     DECIMAL(15,2) NOT NULL,
    s_comment     VARCHAR(101) NOT NULL
) ENGINE = MergeTree() ORDER BY s_suppkey;

CREATE TABLE insert_bench.local_partsupp ON CLUSTER cluster_2S_1R
(
    ps_partkey     INTEGER NOT NULL,
    ps_suppkey     INTEGER NOT NULL,
    ps_availqty    INTEGER NOT NULL,
    ps_supplycost  DECIMAL(15,2)  NOT NULL,
    ps_comment     VARCHAR(199) NOT NULL
) ENGINE = MergeTree() ORDER BY ps_partkey;

CREATE TABLE insert_bench.local_customer ON CLUSTER cluster_2S_1R
(
    c_custkey     INTEGER NOT NULL,
    c_name        VARCHAR(25) NOT NULL,
    c_address     VARCHAR(40) NOT NULL,
    c_nationkey   INTEGER NOT NULL,
    c_phone       CHAR(15) NOT NULL,
    c_acctbal     DECIMAL(15,2)   NOT NULL,
    c_mktsegment  CHAR(10) NOT NULL,
    c_comment     VARCHAR(117) NOT NULL
) ENGINE = MergeTree() ORDER BY c_custkey;

CREATE TABLE insert_bench.local_orders ON CLUSTER cluster_2S_1R
(
    o_orderkey       BIGINT NOT NULL,
    o_custkey        INTEGER NOT NULL,
    o_orderstatus    CHAR(1) NOT NULL,
    o_totalprice     DECIMAL(15,2) NOT NULL,
    o_orderdate      DATE NOT NULL,
    o_orderpriority  CHAR(15) NOT NULL,
    o_clerk          CHAR(15) NOT NULL,
    o_shippriority   INTEGER NOT NULL,
    o_comment        VARCHAR(79) NOT NULL
) ENGINE = MergeTree() ORDER BY o_orderkey;

CREATE TABLE insert_bench.local_lineitem ON CLUSTER cluster_2S_1R
(
    l_orderkey    BIGINT NOT NULL,
    l_partkey     INTEGER NOT NULL,
    l_suppkey     INTEGER NOT NULL,
    l_linenumber  INTEGER NOT NULL,
    l_quantity    DECIMAL(15,2) NOT NULL,
    l_extendedprice  DECIMAL(15,2) NOT NULL,
    l_discount    DECIMAL(15,2) NOT NULL,
    l_tax         DECIMAL(15,2) NOT NULL,
    l_returnflag  CHAR(1) NOT NULL,
    l_linestatus  CHAR(1) NOT NULL,
    l_shipdate    DATE NOT NULL,
    l_commitdate  DATE NOT NULL,
    l_receiptdate DATE NOT NULL,
    l_shipinstruct CHAR(25) NOT NULL,
    l_shipmode     CHAR(10) NOT NULL,
    l_comment      VARCHAR(44) NOT NULL
) ENGINE = MergeTree() ORDER BY l_orderkey;


CREATE TABLE insert_bench.nation ON CLUSTER cluster_2S_1R AS insert_bench.local_nation
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_nation', n_nationkey);

CREATE TABLE insert_bench.region ON CLUSTER cluster_2S_1R AS insert_bench.local_region
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_region', r_regionkey);

CREATE TABLE insert_bench.part ON CLUSTER cluster_2S_1R AS insert_bench.local_part
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_part', p_partkey);

CREATE TABLE insert_bench.supplier ON CLUSTER cluster_2S_1R AS insert_bench.local_supplier
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_supplier', s_suppkey);

CREATE TABLE insert_bench.partsupp ON CLUSTER cluster_2S_1R AS insert_bench.local_partsupp
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_partsupp', ps_partkey);

CREATE TABLE insert_bench.customer ON CLUSTER cluster_2S_1R AS insert_bench.local_customer
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_customer', c_custkey);

CREATE TABLE insert_bench.orders ON CLUSTER cluster_2S_1R AS insert_bench.local_orders
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_orders', o_orderkey);

CREATE TABLE insert_bench.lineitem ON CLUSTER cluster_2S_1R AS insert_bench.local_lineitem
ENGINE = Distributed(cluster_2S_1R, 'insert_bench', 'local_lineitem', l_orderkey);
