# Concord
#
# Copyright (c) 2021 VMware, Inc. All Rights Reserved.
#
# This product is licensed to you under the Apache 2.0 license (the "License").
# You may not use this product except in compliance with the Apache 2.0 License.
#
# This product may include a number of subcomponents with separate copyright
# notices and license terms. Your use of these subcomponents is subject to the
# terms and conditions of the subcomponent's license, as noted in the LICENSE
# file.

import os.path
import random
import unittest
from os import environ

import trio

from util.test_base import ApolloTest
from util import skvbc as kvbc
from util.bft import with_trio, with_bft_network, KEY_FILE_PREFIX, with_constant_load
from util.skvbc_history_tracker import verify_linearizability
import util.eliot_logging as log

SKVBC_INIT_GRACE_TIME = 2

def start_replica_cmd(builddir, replica_id, view_change_timeout_milli="3000"):
    """
    Return a command that starts an skvbc replica when passed to
    subprocess.Popen.
    The replica is started with a short view change timeout.
    Note each arguments is an element in a list.
    The primary replica is started with a Byzantine Strategy, so it
    will exhibit Byzantine behaviours
    """

    status_timer_milli = "500"

    path = os.path.join(builddir, "tests", "simpleKVBC", "TesterReplica", "skvbc_replica")
    cmd = [path,
           "-k", KEY_FILE_PREFIX,
           "-i", str(replica_id),
           "-s", status_timer_milli,
           "-v", view_change_timeout_milli,
           "-x",
           "--enable-req-preprep-from-non-primary"
           ]
    if replica_id == 0 :
        cmd.extend(["-g", "DropPrePreparesNoViewChangeStrategy,DropReadOnlyRepliesStrategy"])

    return cmd
class SkvbcByzantineReadOnlyLivenessAttackTest(ApolloTest):

    __test__ = False  # so that PyTest ignores this test scenario

    @unittest.skip("skip for demo")
    @with_trio
    @with_bft_network(start_replica_cmd, selected_configs=lambda n, f, c: n == 4)
    @verify_linearizability(pre_exec_enabled=True, no_conflicts=True)
    async def test_read_only_liveness_attack_simple(self, bft_network, tracker):
        """
        Use a random client to launch read only requests.
        The Byzantine Primary performs a liveness attack on read only requests
        by isolating f non-faulty replicas to execute the requests this making it difficult
        for the client to gather n - f replies. If the protocol is resilient to the attack
        it shouldn't cause the client to timeout and loose liveness.
        """
        skvbc = kvbc.SimpleKVBCProtocol(bft_network, tracker)
        bft_network.start_all_replicas()
        await trio.sleep(SKVBC_INIT_GRACE_TIME)

        (key, val) = await skvbc.send_write_kv_set()

        client = bft_network.random_client()

        kv_reply = await skvbc.send_read_kv_set(client, key)

        self.assertEqual({key: val}, kv_reply)

    @with_trio
    @with_bft_network(start_replica_cmd, selected_configs=lambda n, f, c: n == 4)
    #@verify_linearizability(pre_exec_enabled=True, no_conflicts=True)
    async def test_read_only_liveness_attack_under_load(self, bft_network):
        """
        Use a random client to launch read only requests.
        The Byzantine Primary performs a liveness attack on read only requests
        by isolating f non-faulty replicas to execute the requests this making it difficult
        for the client to gather n - f replies. If the protocol is resilient to the attack
        it shouldn't cause the client to timeout and loose liveness.
        """
        skvbc = kvbc.SimpleKVBCProtocol(bft_network)
        bft_network.start_all_replicas()
        await trio.sleep(SKVBC_INIT_GRACE_TIME)

        ro_client = bft_network.random_client()
        key = skvbc.random_key()

        num_writes = 0
        num_reads = 0
        num_failed_reads = 0

        async def write():
            nonlocal num_writes
            while True:
                writer = bft_network.random_client(without={ro_client})
                kv = [(key, skvbc.random_value())]
                await skvbc.send_write_kv_set(writer, kv)
                num_writes += 1

        async def read():
            nonlocal num_reads
            nonlocal num_failed_reads
            while True:
                await trio.sleep(0.5)
                reply = await skvbc.send_read_kv_set(ro_client, key)
                num_reads += 1
                if reply is None:
                    num_failed_reads += 1

        with trio.move_on_after(seconds=15):
            async with trio.open_nursery() as nursery:
                nursery.start_soon(write)
                nursery.start_soon(read)

        # Make sure we haven't caused a view change
        await bft_network.wait_for_view(replica_id=0,
                                        expected=lambda v: v == 0,
                                        err_msg="Check if we are still in the initial view.")

        print(f"Results: {num_writes} writes / {num_reads} reads / {num_failed_reads} failed reads")

        self.assertEqual(num_failed_reads, 0, f"Read-only client number of failed reads: {num_failed_reads}")

if __name__ == '__main__':
    unittest.main()
