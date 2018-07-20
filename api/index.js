'use strict'
const express = require('express')
const morgan = require('morgan')
const pkg = require('./package.json')
const axios = require('axios')

const PORT = 4001
const EOS_NODE_API = 'http://mainnet.eoscalgary.io/v1'
const CONTRACT = 'therealkarma'
const TOKEN = 'KARMA'
const DECIMALS = 4
const IGNORED_ACCOUNTS = [
  'therealkarma',
  'karmaairdrop',
  'karma4market'
]

const tokenToNumber = str => {
  const vals = str.split(' ')
  if (vals.length === 2)
    return Number(vals[0]).toFixed(DECIMALS)
  else
    return 0
}

const currencyStatsApi = () => {
  return axios.post(EOS_NODE_API + '/chain/get_currency_stats', {
    code: CONTRACT, symbol: TOKEN
  }).then(res => res.data)
  .catch(err => {
    throw (err.response && err.response.data && err.response.data.error &&
      `${err.response.data.error.code} - ${err.response.data.error.name} - ${err.response.data.error.what}`) ||
      (err.error && err.error.message) ||
      'Unknown Error'
  })
}

const currencyBalanceApi = account => {
  return axios.post(EOS_NODE_API + '/chain/get_currency_balance', {
    code: CONTRACT, symbol: TOKEN, account
  }).then(res => res.data)
}

const calcCirculatingSupply = async total => {
  if (IGNORED_ACCOUNTS.length === 0)
    return total

  try {
    const ignoredSupplies = await Promise.all(IGNORED_ACCOUNTS.map(currencyBalanceApi))

    return ignoredSupplies.reduce((prev, cur) => {
      return (prev - tokenToNumber(cur[0])).toFixed(DECIMALS)
    }, total)

  } catch (err) {
    throw err.message ? err : 'Unknown error getting ignored balances'
  }
}

const app = express()
app.use(morgan('dev'))
app.get('/version', (req, res) => res.status(200).send(pkg.version))

app.get('/stats', (req, res) => {
  currencyStatsApi()
    .then(data => res.status(200).send(data))
    .catch(err => res.status(500).send(err))
})

app.get('/balance/:account', (req, res) => {
  const { account } = req.params

  if (!account)
    return res.status(400).send('Account is mandatory')

  currencyBalanceApi(account)
    .then(data => {

      if (data && data.length)
        return res.status(200).send(tokenToNumber(data[0]).toString())


      throw 'Unknown Balance'
    })
    .catch(err => res.status(500).send(err))
})

app.get('/supply', async (req, res) => {
  try {
    const stats = await currencyStatsApi()

    if (!stats[TOKEN])
      throw('Unknown Supply Response')

    const totalSupply = tokenToNumber(stats[TOKEN].supply)
    const circulatingSupply = await calcCirculatingSupply(totalSupply)
    res.status(200).send(circulatingSupply.toString())

  } catch (err) {
    res.status(500).send(err)
  }
})

app.listen(PORT, () => console.log(`Ready on port ${PORT}.`))
