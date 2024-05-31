## Step 1 Create three-node cluster


```
mkdir ch_cluster
mkdir -p /ch_cluster/chnode1/{config,logs,data,coordination}
mkdir -p /ch_cluster/chnode2/{config,logs,data,coordination}
mkdir -p /ch_cluster/chnode3/{config,logs,data,coordination}
mkdir -p /ch_cluster/bin
```

## Step 2 Create configuration files
add the following configuration files to the config directory of each node

chnode1/config/config.xml
```
<clickhouse>
    <path>/mnt/fast25/ch_cluster/chnode1/data/</path>
    <tmp_path>/mnt/fast25/ch_cluster/chnode1/data/tmp/</tmp_path>
    <user_files_path>/mnt/fast25/ch_cluster/chnode1/data/user_files/</user_files_path>
    <format_schema_path>/mnt/fast25/ch_cluster/chnode1/data/format_schemas/</format_schema_path>
    <access_control_path>/mnt/fast25/ch_cluster/chnode1/data/access/</access_control_path>

    <logger>
        <level>debug</level>
        <log>/mnt/fast25/ch_cluster/chnode1/logs/clickhouse-server.log</log>
        <errorlog>/mnt/fast25/ch_cluster/chnode1/logs/clickhouse-server.err.log</errorlog>
        <size>1000M</size>
        <count>3</count>
    </logger>

    <listen_host>0.0.0.0</listen_host>
    <http_port>8123</http_port>
    <tcp_port>9000</tcp_port>
    <interserver_http_port>9009</interserver_http_port>

    <keeper_server>
        <tcp_port>9181</tcp_port>
        <server_id>1</server_id>
        <log_storage_path>/mnt/fast25/ch_cluster/chnode1/coordination/log</log_storage_path>
        <snapshot_storage_path>/mnt/fast25/ch_cluster/chnode1/coordination/snapshots</snapshot_storage_path>
        <coordination_settings>
            <operation_timeout_ms>10000</operation_timeout_ms>
            <session_timeout_ms>30000</session_timeout_ms>
            <raft_logs_level>trace</raft_logs_level>
        </coordination_settings>
        <raft_configuration>
            <server>
                <id>1</id>
                <hostname>127.0.0.1</hostname>
                <port>9234</port>
            </server>
            <server>
                <id>2</id>
                <hostname>127.0.0.1</hostname>
                <port>9235</port>
            </server>
            <server>
                <id>3</id>
                <hostname>127.0.0.1</hostname>
                <port>9236</port>
            </server>
        </raft_configuration>
    </keeper_server>

    <macros>
        <shard>1</shard>
        <replica>replica_1</replica>
    </macros>

    <remote_servers replace="true">
        <cluster_2S_1R>
            <secret>mysecretphrase</secret>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9000</port>
                </replica>
            </shard>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9001</port>
                </replica>
            </shard>
        </cluster_2S_1R>
    </remote_servers>

    <zookeeper>
        <node index="1">
            <host>127.0.0.1</host>
            <port>9181</port>
        </node>
        <node index="2">
            <host>127.0.0.1</host>
            <port>9182</port>
        </node>
        <node index="3">
            <host>127.0.0.1</host>
            <port>9183</port>
        </node>
    </zookeeper>

    <distributed_ddl>
        <path>/clickhouse/task_queue/ddl</path>
    </distributed_ddl>

    <profiles>
        <default>
            <distributed_product_mode>allow</distributed_product_mode>
        </default>
    </profiles>

    <users>
        <default>
            <profile>default</profile>
            <password></password>
            <networks>
                <ip>::/0</ip>
            </networks>
        </default>
    </users>

    <quotas>
        <default></default>
    </quotas>
</clickhouse>
```

chnode2/config/config.xml
``` 
<clickhouse>
    <path>/mnt/fast25/ch_cluster/chnode2/data/</path>
    <tmp_path>/mnt/fast25/ch_cluster/chnode2/data/tmp/</tmp_path>
    <user_files_path>/mnt/fast25/ch_cluster/chnode2/data/user_files/</user_files_path>
    <format_schema_path>/mnt/fast25/ch_cluster/chnode2/data/format_schemas/</format_schema_path>
    <access_control_path>/mnt/fast25/ch_cluster/chnode2/data/access/</access_control_path>

    <logger>
        <level>debug</level>
        <log>/mnt/fast25/ch_cluster/chnode2/logs/clickhouse-server.log</log>
        <errorlog>/mnt/fast25/ch_cluster/chnode2/logs/clickhouse-server.err.log</errorlog>
        <size>1000M</size>
        <count>3</count>
    </logger>

    <listen_host>0.0.0.0</listen_host>
    <http_port>8124</http_port>
    <tcp_port>9001</tcp_port>
    <interserver_http_port>9010</interserver_http_port>

    <keeper_server>
        <tcp_port>9182</tcp_port>
        <server_id>2</server_id>
        <log_storage_path>/mnt/fast25/ch_cluster/chnode2/coordination/log</log_storage_path>
        <snapshot_storage_path>/mnt/fast25/ch_cluster/chnode2/coordination/snapshots</snapshot_storage_path>
        <coordination_settings>
            <operation_timeout_ms>10000</operation_timeout_ms>
            <session_timeout_ms>30000</session_timeout_ms>
            <raft_logs_level>trace</raft_logs_level>
        </coordination_settings>
        <raft_configuration>
            <server>
                <id>1</id>
                <hostname>127.0.0.1</hostname>
                <port>9234</port>
            </server>
            <server>
                <id>2</id>
                <hostname>127.0.0.1</hostname>
                <port>9235</port>
            </server>
            <server>
                <id>3</id>
                <hostname>127.0.0.1</hostname>
                <port>9236</port>
            </server>
        </raft_configuration>
    </keeper_server>

    <macros>
        <shard>2</shard>
        <replica>replica_1</replica>
    </macros>

    <remote_servers replace="true">
        <cluster_2S_1R>
            <secret>mysecretphrase</secret>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9000</port>
                </replica>
            </shard>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9001</port>
                </replica>
            </shard>
        </cluster_2S_1R>
    </remote_servers>

    <zookeeper>
        <node index="1">
            <host>127.0.0.1</host>
            <port>9181</port>
        </node>
        <node index="2">
            <host>127.0.0.1</host>
            <port>9182</port>
        </node>
        <node index="3">
            <host>127.0.0.1</host>
            <port>9183</port>
        </node>
    </zookeeper>

    <distributed_ddl>
        <path>/clickhouse/task_queue/ddl</path>
    </distributed_ddl>

    <profiles>
        <default>
            <distributed_product_mode>allow</distributed_product_mode>
        </default>
    </profiles>

    <users>
        <default>
            <profile>default</profile>
            <password></password>
            <networks>
                <ip>::/0</ip>
            </networks>
        </default>
    </users>

    <quotas>
        <default></default>
    </quotas>
</clickhouse>
```

chnode3/config/config.xml
```
<clickhouse>
    <path>/mnt/fast25/ch_cluster/chnode3/data/</path>
    <tmp_path>/mnt/fast25/ch_cluster/chnode3/data/tmp/</tmp_path>
    <user_files_path>/mnt/fast25/ch_cluster/chnode3/data/user_files/</user_files_path>
    <format_schema_path>/mnt/fast25/ch_cluster/chnode3/data/format_schemas/</format_schema_path>
    <access_control_path>/mnt/fast25/ch_cluster/chnode3/data/access/</access_control_path>

    <logger>
        <level>debug</level>
        <log>/mnt/fast25/ch_cluster/chnode3/logs/clickhouse-server.log</log>
        <errorlog>/mnt/fast25/ch_cluster/chnode3/logs/clickhouse-server.err.log</errorlog>
        <size>1000M</size>
        <count>3</count>
    </logger>

    <listen_host>0.0.0.0</listen_host>
    <http_port>8125</http_port>
    <tcp_port>9002</tcp_port>
    <interserver_http_port>9011</interserver_http_port>

    <keeper_server>
        <tcp_port>9183</tcp_port>
        <server_id>3</server_id>
        <log_storage_path>/mnt/fast25/ch_cluster/chnode3/coordination/log</log_storage_path>
        <snapshot_storage_path>/mnt/fast25/ch_cluster/chnode3/coordination/snapshots</snapshot_storage_path>
        <coordination_settings>
            <operation_timeout_ms>10000</operation_timeout_ms>
            <session_timeout_ms>30000</session_timeout_ms>
            <raft_logs_level>trace</raft_logs_level>
        </coordination_settings>
        <raft_configuration>
            <server>
                <id>1</id>
                <hostname>127.0.0.1</hostname>
                <port>9234</port>
            </server>
            <server>
                <id>2</id>
                <hostname>127.0.0.1</hostname>
                <port>9235</port>
            </server>
            <server>
                <id>3</id>
                <hostname>127.0.0.1</hostname>
                <port>9236</port>
            </server>
        </raft_configuration>
    </keeper_server>

    <macros>
        <shard>3</shard>
        <replica>replica_1</replica>
    </macros>

    <remote_servers replace="true">
        <cluster_2S_1R>
            <secret>mysecretphrase</secret>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9000</port>
                </replica>
            </shard>
            <shard>
                <internal_replication>true</internal_replication>
                <replica>
                    <host>127.0.0.1</host>
                    <port>9001</port>
                </replica>
            </shard>
        </cluster_2S_1R>
    </remote_servers>

    <zookeeper>
        <node index="1">
            <host>127.0.0.1</host>
            <port>9181</port>
        </node>
        <node index="2">
            <host>127.0.0.1</host>
            <port>9182</port>
        </node>
        <node index="3">
            <host>127.0.0.1</host>
            <port>9183</port>
        </node>
    </zookeeper>

    <distributed_ddl>
        <path>/clickhouse/task_queue/ddl</path>
    </distributed_ddl>

    <profiles>
        <default>
            <distributed_product_mode>allow</distributed_product_mode>
        </default>
    </profiles>

    <users>
        <default>
            <profile>default</profile>
            <password></password>
            <networks>
                <ip>::/0</ip>
            </networks>
        </default>
    </users>

    <quotas>
        <default></default>
    </quotas>
</clickhouse>
```

## Step 3 Install Clikchouse Locally
Following the instructions in the link below to build clickhouse from the source
```
https://clickhouse.com/docs/en/install
```
Then put binary files "clickhouse-server" and "clickhouser-client" from the clickhouse build directory to "ch_cluster/bin"

## Step 4 Generate Data
Clone the tpch-scripts repository from the link below
```
https://github.com/MonetDBSolutions/tpch-scripts
```
And inside the tpch-scripts directory run the following command to generate data
```
./tpch_build.sh -s 100 -f /path/to/farm
```

## Step 5 Start the cluster and create tables
Start the cluster by running the following commands
```
/ch_cluster/bin/clickhouse-server --config /ch_cluster/chnode1/config/config.xml
/ch_cluster/bin/clickhouse-server --config /ch_cluster/chnode2/config/config.xml
/ch_cluster/bin/clickhouse-server --config /ch_cluster/chnode3/config/config.xml

/ch_cluster/bin/clickhouse-client --host 
```

Create distributed tables by running the sql file "tpch_ch_schema distributed.sql"

Then insert data to the tables from the path where you generated in step 4

Step 5 Profiling and Analysis
Clond the tpch-clickhouse repository from the link below
```
https://github.com/MonetDB/tpch-clickhouse
```
Run the following command to run the queries for 10 times and benchmakr the performance
```
./ch_horizontal_run.sh --db tpch_benchmark -n 10 
```

Specifically, to profile the syscall for a single query, run the following command
```
sudo perf record -g -p {pid of the node of your interest} -- /ch_cluster/bin/clickhouse-client  --port {port number of the node of your interest} --database=tpch_benchmark < {sql file}
```

Then draw the flamegraph following the instructions in the link below
```
https://github.com/brendangregg/FlameGraph?tab=readme-ov-file
```

