import {
    ABISerializableObject,
    Asset,
    BlockTimestamp,
    Bytes,
    Checksum256,
    Name,
    PrivateKey,
    Serializer,
    UInt64,
} from '@wharfkit/antelope'
import {TimePointSec} from '@greymass/eosio'
import {Blockchain, expectToThrow} from '@proton/vert'
import {beforeEach, describe, expect, test} from 'bun:test'

import * as EpochContract from '../build/epoch.drops.ts'
import * as DropsContract from '../codegen/drops.ts'
import * as TokenContract from '../codegen/eosio.token.ts'

// Vert EOS VM
const blockchain = new Blockchain()
const bob = 'bob'
const alice = 'alice'
const charlie = 'charlie'
blockchain.createAccounts(bob, alice)

const core_contract = 'epoch.drops'
const contracts = {
    epoch: blockchain.createContract(core_contract, `build/${core_contract}`, true),
    core: blockchain.createContract('drops', 'include/drops/drops', true),
    token: blockchain.createContract('eosio.token', 'include/eosio.token/eosio.token', true),
    fake: blockchain.createContract('fake.token', 'include/eosio.token/eosio.token', true),
    system: blockchain.createContract('eosio', 'include/eosio.system/eosio', true),
}

function toBeStruct(x, struct: ABISerializableObject) {
    return {
        pass: (x as ABISerializableObject).equals(struct),
        message: () =>
            'expected \n ' +
            JSON.stringify(x, null, 2) +
            '\n\nto equal \n' +
            JSON.stringify(struct, null, 2),
    }
}

expect.extend({
    toBeStruct,
})

function getState(): EpochContract.Types.state_row {
    const scope = Name.from(core_contract).value.value
    return EpochContract.Types.state_row.from(contracts.epoch.tables.state(scope).getTableRows()[0])
}

// function getBalance(account: string) {
//     const scope = Name.from(account).value.value
//     const primary_key = Asset.SymbolCode.from('EOS').value.value
//     return Asset.from(contracts.token.tables.accounts(scope).getTableRow(primary_key).balance)
// }

function getEpoch(epoch: bigint): EpochContract.Types.epoch_row {
    const scope = Name.from(core_contract).value.value
    const row = contracts.epoch.tables.epoch(scope).getTableRow(epoch)
    if (!row) throw new Error('Epoch not found')
    return EpochContract.Types.epoch_row.from(row)
}

function getEpochs(): EpochContract.Types.epoch_row[] {
    const rows = contracts.epoch.tables.epoch().getTableRows()
    return rows.map((row) => EpochContract.Types.epoch_row.from(row))
}

function getCommits(): EpochContract.Types.commit_row[] {
    const rows = contracts.epoch.tables.commit().getTableRows()
    return rows.map((row) => EpochContract.Types.commit_row.from(row))
}

function getReveals(): EpochContract.Types.reveal_row[] {
    const rows = contracts.epoch.tables.reveal().getTableRows()
    return rows.map((row) => EpochContract.Types.reveal_row.from(row))
}

// function getDrop(seed: bigint): DropsContract.Types.drop_row {
//     const scope = Name.from(core_contract).value.value
//     const row = contracts.core.tables.drop(scope).getTableRow(seed)
//     if (!row) throw new Error('Drop not found')
//     return DropsContract.Types.drop_row.from(row)
// }

// function getDrops(owner?: string): DropsContract.Types.drop_row[] {
//     const scope = Name.from(core_contract).value.value
//     const rows = contracts.core.tables.drop(scope).getTableRows()
//     if (!owner) return rows
//     return rows.filter((row) => row.owner === owner)
// }

// function getUnbind(owner?: string): DropsContract.Types.unbind_row[] {
//     const scope = Name.from(core_contract).value.value
//     const rows = contracts.core.tables.unbind(scope).getTableRows()
//     if (!owner) return rows
//     return rows.filter((row) => row.owner === owner)
// }

function getOracles(): EpochContract.Types.oracle_row[] {
    const scope = Name.from(core_contract).value.value
    return contracts.epoch.tables
        .oracle(scope)
        .getTableRows()
        .map((row) => EpochContract.Types.oracle_row.from(row))
}

// const ERROR_INVALID_MEMO = `eosio_assert_message: Invalid transfer memo. (ex: "<amount>,<data>")`
// const ERROR_SYSTEM_DISABLED = 'eosio_assert_message: Drops system is disabled.'

const datetime = new Date('2024-01-29T00:00:00.000')
function advanceTime(seconds: number) {
    const newDate = new Date(datetime.getTime() + seconds * 1000)
    blockchain.setTime(TimePointSec.from(newDate))
}

const defaultState = {
    genesis: BlockTimestamp.from(datetime),
    duration: 86400,
    enabled: false,
}

// Sample random data (just a random key)
const mockSecret = 'PVT_K1_dqeryoVTjBmXPtikBkjCFD4EMM1YdZTLQKqip8XUQHWyj9ZSD'
const mockReveal = Checksum256.hash(Bytes.from(mockSecret, 'utf8').array).hexString
const mockCommit = Checksum256.hash(Bytes.from(mockReveal, 'utf8').array).hexString

describe(core_contract, () => {
    // Setup before each test
    beforeEach(async () => {
        blockchain.setTime(TimePointSec.from(datetime))
        await contracts.epoch.actions.wipe().send()
    })

    test('state::default', async () => {
        expect(getState()).toBeStruct(defaultState)
        expect(() => getEpoch(1n)).toThrow('Epoch not found')
    })

    test('init', async () => {
        await contracts.epoch.actions.addoracle([alice]).send()
        await contracts.epoch.actions.init().send()
        expect(getEpoch(1n)).toBeStruct({
            epoch: 1n,
            seed: '0000000000000000000000000000000000000000000000000000000000000000',
            oracles: [alice],
        })
    })

    test('enable', async () => {
        await contracts.epoch.actions.enable([true]).send()
        expect(getState()).toBeStruct({
            ...defaultState,
            enabled: true,
        })
        await contracts.epoch.actions.enable([false]).send()
        expect(getState()).toBeStruct({
            ...defaultState,
            enabled: false,
        })
    })

    test('duration', async () => {
        await contracts.epoch.actions.duration([888888]).send()
        expect(getState()).toBeStruct({
            ...defaultState,
            duration: 888888,
        })
    })

    describe('oracle', () => {
        test('addoracle', async () => {
            await contracts.epoch.actions.addoracle([alice]).send()
            const rows = getOracles()
            expect(rows.length).toBe(1)
            expect(rows[0]).toBeStruct({oracle: 'alice'})
        })

        test('removeoracle', async () => {
            await contracts.epoch.actions.addoracle([alice]).send()
            const beforeRemove = getOracles()
            expect(beforeRemove.length).toBe(1)
            await contracts.epoch.actions.removeoracle([alice]).send()
            const afterRemove = getOracles()
            expect(afterRemove.length).toBe(0)
        })
    })

    describe('commit', () => {
        beforeEach(async () => {
            await contracts.epoch.actions.addoracle([alice]).send()
            await contracts.epoch.actions.init().send()
        })
        test('success', async () => {
            await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
            const commits = getCommits()
            expect(commits.length).toBe(1)
            expect(commits[0]).toBeStruct({
                id: 0,
                epoch: 1,
                oracle: 'alice',
                commit: mockCommit,
            })
        })
        test('advances epoch', async () => {
            await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
            const commits = getCommits()
            expect(commits.length).toBe(1)
            const epochs = getEpochs()
            expect(epochs.length).toBe(1)

            advanceTime(86400)

            await contracts.epoch.actions.commit([alice, 2, mockCommit]).send(alice)
            const epochsTomorrow = getEpochs()
            expect(epochsTomorrow.length).toBe(2)
            const commitsTomorrow = getCommits()
            expect(commitsTomorrow.length).toBe(2)
        })

        describe('errors', () => {
            test('disabled', async () => {
                await contracts.epoch.actions.enable([false]).send()
                const action = contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                expect(action).rejects.toThrow('eosio_assert_message: Drops system is disabled.')
            })
            test('already committed', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                const action = contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                expect(action).rejects.toThrow('eosio_assert: Oracle has already committed')
            })
            test('not in epoch', async () => {
                const action = contracts.epoch.actions.commit([bob, 1, mockCommit]).send(bob)
                expect(action).rejects.toThrow(
                    'eosio_assert_message: Oracle is not in the list of oracles for Epoch 1.'
                )
            })
            test('invalid auth', async () => {
                const action = contracts.epoch.actions.commit([alice, 1, mockCommit]).send(bob)
                expect(action).rejects.toThrow('missing required authority alice')
            })
            test('invalid epoch', async () => {
                const action = contracts.epoch.actions.commit([alice, 2, mockCommit]).send(alice)
                expect(action).rejects.toThrow(
                    'eosio_assert_message: Epoch submitted (2) is not the current epoch (1).'
                )
            })
            test('cannot commit to epoch which has ended', async () => {
                advanceTime(86400)
                const action = contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                expect(action).rejects.toThrow(
                    'eosio_assert_message: Epoch submitted (1) is not the current epoch (2).'
                )
            })
            test('cannot advance with no oracles', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                await contracts.epoch.actions.removeoracle([alice]).send()
                advanceTime(86400)
                const action = contracts.epoch.actions.commit([alice, 2, mockCommit]).send(alice)
                expect(action).rejects.toThrow('eosio_assert: No active oracles')
            })
        })
    })

    describe('reveal', () => {
        beforeEach(async () => {
            await contracts.epoch.actions.addoracle([alice]).send()
            await contracts.epoch.actions.init().send()
        })
        test('success', async () => {
            await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)

            advanceTime(86400)

            await contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)

            const reveals = getReveals()
            expect(reveals.length).toBe(1)

            const epoch = getEpoch(1n)
            expect(
                epoch.seed.equals(
                    'aa64858f9aef574443d0595ef57665d4252475b3f9a5a484c40654401a4116e5'
                )
            ).toBeTrue()
        })
        test('does not reveal until all oracles submit', async () => {
            // reinitialize with two oracles
            await contracts.epoch.actions.wipe().send()
            await contracts.epoch.actions.addoracle([alice]).send()
            await contracts.epoch.actions.addoracle([bob]).send()
            await contracts.epoch.actions.init().send()

            await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
            await contracts.epoch.actions.commit([bob, 1, mockCommit]).send(bob)

            advanceTime(86400)

            await contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)

            const reveals = getReveals()
            expect(reveals.length).toBe(1)

            const epoch = getEpoch(1n)
            expect(
                epoch.seed.equals(
                    '0000000000000000000000000000000000000000000000000000000000000000'
                )
            ).toBeTrue()

            await contracts.epoch.actions.reveal([bob, 1, mockReveal]).send(bob)

            const revealsAfter = getReveals()
            expect(revealsAfter.length).toBe(2)

            const epochAfter = getEpoch(1n)
            expect(
                epochAfter.seed.equals(
                    '2202d9f6ff083b8061e9741c7a03392ded42acbfc563ef1ad400386af8f38ff5'
                )
            ).toBeTrue()
        })

        describe('errors', () => {
            test('disabled', async () => {
                await contracts.epoch.actions.enable([false]).send()
                const action = contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)
                expect(action).rejects.toThrow('eosio_assert_message: Drops system is disabled.')
            })
            test('reveal does not match commit', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                advanceTime(86400)
                const action = contracts.epoch.actions.reveal([alice, 1, 'foo']).send(alice)
                expect(action).rejects.toThrow(
                    "eosio_assert_message: Reveal value 'foo' hashes to '2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae' which does not match commit value '3d1f01d81f9d605b2da582fbf5a4110ef35477caf7f2c5ae4fa0e3877ed16747'."
                )
            })
            test('epoch has not ended', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                const action = contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)
                expect(action).rejects.toThrow('eosio_assert_message: Epoch (1) has not completed.')
            })
            test('oracle already revealed', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                advanceTime(86400)
                await contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)
                const action = contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(alice)
                expect(action).rejects.toThrow('eosio_assert: Oracle has already revealed')
            })
            test('epoch doesnt exist', async () => {
                const action = contracts.epoch.actions.reveal([alice, 10, mockReveal]).send(alice)
                expect(action).rejects.toThrow('eosio_assert_message: Epoch 10 does not exist.')
            })
            test('oracle not included in epoch', async () => {
                const action = contracts.epoch.actions.reveal([bob, 1, mockReveal]).send(bob)
                expect(action).rejects.toThrow(
                    'eosio_assert_message: Oracle is not in the list of oracles for Epoch 1.'
                )
            })
            test('invalid auth', async () => {
                const action = contracts.epoch.actions.reveal([alice, 1, mockReveal]).send(bob)
                expect(action).rejects.toThrow('missing required authority alice')
            })
        })
    })

    describe('admin check', () => {
        test('addoracle', async () => {
            const action = contracts.epoch.actions.addoracle([bob]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
        test('enable', async () => {
            const action = contracts.epoch.actions.enable([true]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
        test('duration', async () => {
            const action = contracts.epoch.actions.duration([888888]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
        test('removeoracle', async () => {
            const action = contracts.epoch.actions.removeoracle([alice]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
    })
})
