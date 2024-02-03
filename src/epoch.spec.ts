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

function getCommits(): EpochContract.Types.epoch_row[] {
    const rows = contracts.epoch.tables.commit().getTableRows()
    return rows.map((row) => EpochContract.Types.commit_row.from(row))
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
const defaultState = {
    genesis: BlockTimestamp.from(datetime),
    epochlength: 86400,
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
            completed: false,
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

    test('epochlength', async () => {
        await contracts.epoch.actions.epochlength([888888]).send()
        expect(getState()).toBeStruct({
            ...defaultState,
            epochlength: 888888,
        })
    })

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

            const tomorrow = new Date(datetime.getTime() + 86400 * 1000)
            blockchain.setTime(TimePointSec.from(tomorrow))

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
                    'eosio_assert: Oracle is not in the list of oracles for this epoch'
                )
            })
            test('invalid auth', async () => {
                const action = contracts.epoch.actions.commit([alice, 1, mockCommit]).send(bob)
                expect(action).rejects.toThrow('missing required authority alice')
            })
            test('invalid epoch', async () => {
                const action = contracts.epoch.actions.commit([alice, 2, mockCommit]).send(alice)
                expect(action).rejects.toThrow(
                    'eosio_assert: Epoch does not exist in oracle contract'
                )
            })
            test('cannot advance with no oracles', async () => {
                await contracts.epoch.actions.commit([alice, 1, mockCommit]).send(alice)
                await contracts.epoch.actions.removeoracle([alice]).send()
                const tomorrow = new Date(datetime.getTime() + 86400 * 1000)
                blockchain.setTime(TimePointSec.from(tomorrow))
                const action = contracts.epoch.actions.commit([alice, 2, mockCommit]).send(alice)
                expect(action).rejects.toThrow('eosio_assert: No active oracles - cannot advance.')
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
        test('epochlength', async () => {
            const action = contracts.epoch.actions.epochlength([888888]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
        test('removeoracle', async () => {
            const action = contracts.epoch.actions.removeoracle([alice]).send(alice)
            expect(action).rejects.toThrow('missing required authority epoch.drops')
        })
    })

    // test('on_transfer', async () => {
    //     const before = getBalance(alice)
    //     await contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', '10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'])
    //         .send(alice)
    //     const after = getBalance(alice)

    //     expect(after.units.value - before.units.value).toBe(-5847)
    //     expect(getDrops(alice).length).toBe(10)
    //     expect(
    //         getDrop(6530728038117924388n).equals({
    //             seed: '6530728038117924388',
    //             owner: 'alice',
    //             created: '2024-01-29T00:00:00.000',
    //             bound: false,
    //         })
    //     ).toBeTrue()
    // })

    // test('on_transfer::error - contract disabled', async () => {
    //     await contracts.core.actions.enable([false]).send()
    //     const action = contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', '10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'])
    //         .send(alice)
    //     expect(action).rejects.toThrow(ERROR_SYSTEM_DISABLED)
    // })

    // test('on_transfer::error - empty memo', async () => {
    //     const action = contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', ''])
    //         .send(alice)
    //     expect(action).rejects.toThrow(ERROR_INVALID_MEMO)
    // })

    // test('on_transfer::error - invalid number', async () => {
    //     const action = contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', 'foo,bar'])
    //         .send(alice)
    //     expect(action).rejects.toThrow('eosio_assert: invalid number format or overflow')
    // })

    // test('on_transfer::error - number underflow', async () => {
    //     const action = contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', '-1,bar'])
    //         .send(alice)
    //     expect(action).rejects.toThrow('eosio_assert: number underflow')
    // })

    // test('on_transfer::error - at least 32 characters', async () => {
    //     const action = contracts.token.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', '1,foobar'])
    //         .send(alice)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert: Drop data must be at least 32 characters in length.'
    //     )
    // })

    // test('mint', async () => {
    //     await contracts.core.actions.mint([bob, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb']).send(bob)

    //     expect(getDrops(bob).length).toBe(1)
    //     expect(
    //         getDrop(10272988527514872302n).equals({
    //             seed: '10272988527514872302',
    //             owner: 'bob',
    //             created: '2024-01-29T00:00:00.000',
    //             bound: true,
    //         })
    //     ).toBeTrue()
    // })

    // test('mint::error - already exists', async () => {
    //     const action = contracts.core.actions
    //         .mint([bob, 1, 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb'])
    //         .send(bob)

    //     await expectToThrow(
    //         action,
    //         'eosio_assert_message: Drop 10272988527514872302 already exists.'
    //     )
    // })

    // test('mint 1K', async () => {
    //     await contracts.core.actions.mint([bob, 1000, 'cccccccccccccccccccccccccccccccc']).send(bob)

    //     expect(getDrops(bob).length).toBe(1001)
    // })

    // test('on_transfer::error - invalid contract', async () => {
    //     const action = contracts.fake.actions
    //         .transfer([alice, core_contract, '10.0000 EOS', '10,aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'])
    //         .send(alice)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert: Only the eosio.token contract may send tokens to this contract.'
    //     )
    // })

    // test('destroy', async () => {
    //     const before = getBalance(alice)
    //     await contracts.core.actions
    //         .destroy([alice, ['6530728038117924388', '8833355934996727321'], 'memo'])
    //         .send(alice)
    //     const after = getBalance(alice)
    //     const transfer = TokenContract.Types.transfer.from(blockchain.actionTraces[2].decodedData)

    //     expect(transfer.quantity.units.value.toNumber()).toBe(1157)
    //     expect(transfer.memo).toBe('Reclaimed RAM value of 2 drops(s)')
    //     expect(after.units.value - before.units.value).toBe(1157)
    //     expect(getDrops(alice).length).toBe(8)
    //     expect(() => getDrop(6530728038117924388n)).toThrow('Drop not found')
    // })

    // test('destroy::error - not found', async () => {
    //     const action = contracts.core.actions.destroy([alice, ['123'], 'memo']).send(alice)
    //     expect(action).rejects.toThrow('eosio_assert: Drop not found.')
    // })

    // test('destroy::error - must belong to owner', async () => {
    //     const action = contracts.core.actions
    //         .destroy([bob, ['17855725969634623351'], 'memo'])
    //         .send(bob)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert_message: Drop 17855725969634623351 does not belong to account.'
    //     )
    // })

    // test('destroy::error - missing required authority', async () => {
    //     const action = contracts.core.actions
    //         .destroy([bob, ['17855725969634623351'], 'memo'])
    //         .send(alice)
    //     expect(action).rejects.toThrow('missing required authority bob')
    // })

    // test('unbind', async () => {
    //     const before = getBalance(bob)
    //     expect(getDrop(10272988527514872302n).bound).toBeTruthy()
    //     await contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)
    //     await contracts.token.actions
    //         .transfer([bob, core_contract, '10.0000 EOS', 'unbind'])
    //         .send(bob)

    //     // drop must now be unbound
    //     expect(getDrop(10272988527514872302n).bound).toBeFalsy()
    //     const after = getBalance(bob)

    //     // EOS returned for excess RAM
    //     expect(after.units.value - before.units.value).toBe(-583)
    // })

    // test('unbind::error - not found', async () => {
    //     const action = contracts.core.actions.unbind([bob, ['123']]).send(bob)
    //     expect(action).rejects.toThrow('eosio_assert: Drop not found.')
    // })

    // test('unbind::error - does not belong to account', async () => {
    //     const action = contracts.core.actions.unbind([alice, ['10272988527514872302']]).send(alice)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert_message: Drop 10272988527514872302 does not belong to account.'
    //     )
    // })

    // test('unbind::error - is not bound', async () => {
    //     const action = contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)
    //     expect(action).rejects.toThrow('eosio_assert_message: Drop 10272988527514872302 is not bound')
    // })

    // test('bind', async () => {
    //     const before = getBalance(bob)
    //     expect(getDrop(10272988527514872302n).bound).toBeFalsy()
    //     await contracts.core.actions.bind([bob, ['10272988527514872302']]).send(bob)

    //     // drop must now be unbound
    //     expect(getDrop(10272988527514872302n).bound).toBeTruthy()
    //     const after = getBalance(bob)

    //     // EOS returned for excess RAM
    //     expect(after.units.value - before.units.value).toBe(578)
    // })

    // test('bind::error - not found', async () => {
    //     const action = contracts.core.actions.bind([bob, ['123']]).send(bob)
    //     expect(action).rejects.toThrow('eosio_assert: Drop not found.')
    // })

    // test('bind::error - does not belong to account', async () => {
    //     const action = contracts.core.actions.bind([alice, ['10272988527514872302']]).send(alice)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert_message: Drop 10272988527514872302 does not belong to account.'
    //     )
    // })

    // test('bind::error - is not unbound', async () => {
    //     const action = contracts.core.actions.bind([bob, ['10272988527514872302']]).send(bob)
    //     await expectToThrow(
    //         action,
    //         'eosio_assert_message: Drop 10272988527514872302 is not unbound'
    //     )
    // })

    // test('cancelunbind', async () => {
    //     expect(getUnbind(bob).length).toBe(0)
    //     await contracts.core.actions.unbind([bob, ['10272988527514872302']]).send(bob)
    //     expect(getUnbind(bob).length).toBe(1)
    //     await contracts.core.actions.cancelunbind([bob]).send(bob)
    //     expect(getUnbind(bob).length).toBe(0)
    // })

    // test('cancelunbind::error - No unbind request', async () => {
    //     const action = contracts.core.actions.cancelunbind([bob]).send(bob)
    //     expect(action).rejects.toThrow('eosio_assert: No unbind request found for account.')
    // })
})
