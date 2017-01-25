import AWS from 'aws-sdk'
import Geolib from 'geolib'

AWS.config.update({
  region: 'us-west-1'
  // endpoint: 'http://localhost:8000'
})

// const dynamodb = new AWS.DynamoDB()
// const SoulDistanceLocationTable = {
//   TableName: 'SoulDistanceLocation',
//   KeySchema: [
//     {AttributeName: 'Device', KeyType: 'HASH'},
//     {AttributeName: 'UpdateUTC', KeyType: 'RANGE'}
//   ],
//   AttributeDefinitions: [
//     {AttributeName: 'Device', AttributeType: 'N'},
//     {AttributeName: 'UpdateUTC', AttributeType: 'N'}
//   ],
//   ProvisionedThroughput: {
//     ReadCapacityUnits: 10,
//     WriteCapacityUnits: 10
//   }
// }
// dynamodb.createTable(SoulDistanceLocationTable, (err, data) => {
//   if (err) {
//     console.error('Unable to create table. Error JSON:', JSON.stringify(err, null, 2))
//   } else {
//     console.log('Created table. Table description JSON:', JSON.stringify(data, null, 2))
//   }
// })

const DocumentClient = new AWS.DynamoDB.DocumentClient()

const PutDeviceLocation = async ({Device, Latitude, Longitude}) => {
  const DeviceLocation = {
    TableName: 'SoulDistanceLocation',
    Item: {
      'Device': Device,
      'UpdateUTC': new Date().getTime(),
      'Latitude': Latitude,
      'Longitude': Longitude
    }
  }
  const PutResult = () => new Promise((resolve, reject) => {
    DocumentClient.put(DeviceLocation, (err, data) => {
      if (err) { reject({status: 520, err}) }
      resolve(data)
    })
  })
  return await PutResult()
}

const QueryDeviceLocation = async ({Device}) => {
  const DeviceLocationQuery = {
    TableName: 'SoulDistanceLocation',
    KeyConditionExpression: 'Device = :Device',
    ExpressionAttributeValues: {
      ':Device': Device
    },
    ScanIndexForward: false,
    Limit: 1
  }
  const QueryResult = () => new Promise((resolve, reject) => {
    DocumentClient.query(DeviceLocationQuery, (err, data) => {
      if (err) { reject({status: 521, err}) }
      resolve(data)
    })
  })
  const data = await QueryResult()
  if (!data.Items || data.Items.length < 1) {
    const err = {status: 404, err: new Error('No result')}
    throw err
  }
  return data.Items[0]
}

const HandlerIndexGet = async() => {
  return 'Soul Distance API Server'
}

const HandlerUTCGet = async() => {
  return (new Date().getTime()).toString()
}

const HandlerLocationPost = async({Device, Latitude, Longitude}) => {
  try {
    const PutResult = await PutDeviceLocation({Device, Latitude, Longitude})
    console.log('Added item:', JSON.stringify(PutResult, null, 2))
  } catch (err) {
    console.error('Unable to add item.')
    throw err
  }
  return 'SUCCESS'
}

// const HandlerLocationGet = async({Device}) => {
//   try {
//     const DeviceLocation = await QueryDeviceLocation({Device})
//     console.log('Query succeeded.', DeviceLocation)
//   } catch (err) {
//     console.log('Unable to query.')
//     throw err
//   }
// }

const HandlerDistanceGet = async({Local, Remote}) => {
  const LocalDeviceLocation = await QueryDeviceLocation({Device: Local})
  const RemoteDeviceLocation = await QueryDeviceLocation({Device: Remote})
  const Distance = Geolib.getDistance(
    { latitude: LocalDeviceLocation.Latitude, longitude: LocalDeviceLocation.Longitude },
    { latitude: RemoteDeviceLocation.Latitude, longitude: RemoteDeviceLocation.Longitude },
  )
  return JSON.stringify({
    LocalUpdateUTC: LocalDeviceLocation.UpdateUTC,
    RemoteUpdateUTC: RemoteDeviceLocation.UpdateUTC,
    Distance: Distance
  })
}

const Router = async({event, context, callback}) => {
  console.log('event:')
  console.log(event)
  console.log('context:')
  console.log(context)
  console.log(`Switching event.resource ${event.resource}`)
  switch (event.resource) {
    case '/':
      switch (event.httpMethod) {
        case 'GET':
          return await HandlerIndexGet()
      }
      return null
    case '/UTC':
      switch (event.httpMethod) {
        case 'GET':
          return await HandlerUTCGet()
      }
      return null
    case '/Location/{Device}':
      switch (event.httpMethod) {
        case 'POST':
          const body = JSON.parse(event.body)
          return await HandlerLocationPost({
            Device: Number(event.pathParameters.Device),
            Latitude: Number(body.Latitude),
            Longitude: Number(body.Longitude)})
      }
      return null
    case '/Distance':
      switch (event.httpMethod) {
        case 'GET':
          return await HandlerDistanceGet({
            Local: Number(event.queryStringParameters.Local), 
            Remote: Number(event.queryStringParameters.Remote)})
      }
      return null
  }
  return null
}

export const handler = (event, context, callback) => {
  Router({event, context, callback}).then((response) => {
    console.log('response:')
    console.log(response)
    if (response) {
      callback(null, {statusCode: 200, body: response.toString()})
    } else {
      callback({statusCode: 404, body: 'Not Found'}, null)
    }
  }).catch((err) => {
    console.log('err:')
    console.log(err)
    callback({statusCode: err.status, body: err.err.toString()}, null)
  })
}

