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

def start_replica_cmd(builddir, replica_id, view_change_timeout_milli="10000"):
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
           "-x"
           ]
    if replica_id == 0 :
        cmd.extend(["-g", "DropPrePreparesNoViewChangeStrategy,DropReadOnlyRepliesStrategy"])

    return cmd
class SkvbcByzantineReadOnlyLivenessAttackTest(ApolloTest):

    __test__ = False  # so that PyTest ignores this test scenario

    @with_trio
    @with_bft_network(start_replica_cmd, selected_configs=lambda n, f, c: f >= 1)
    @verify_linearizability(pre_exec_enabled=True, no_conflicts=True)
    async def test_byzantine_behavior(self, bft_network, tracker):
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

if __name__ == '__main__':
    unittest.main()
