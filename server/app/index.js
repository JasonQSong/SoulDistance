import AWS from 'aws-sdk'
import Geolib from 'geolib'

AWS.config.update({
  region: 'us-west-1',
  endpoint: 'http://localhost:8000'
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
      'UpdateUTC': Date.now(),
      'Latitude': Latitude,
      'Longitude': Longitude
    }
  }
  const PutResult = () => new Promise((resolve, reject) => {
    DocumentClient.put(DeviceLocation, (err, data) => {
      if (err) { reject({status: 501, err}) }
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
      if (err) { reject({status: 501, err}) }
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
  return Date.now()
}

const HandlerLocationPost = async({Device, Latitude, Longitude}) => {
  try {
    const PutResult = await PutDeviceLocation({Device, Latitude, Longitude})
    console.log('Added item:', JSON.stringify(PutResult, null, 2))
  } catch (err) {
    console.error('Unable to add item.')
    throw err
  }
}

const HandlerLocationGet = async({Device}) => {
  try {
    const DeviceLocation = await QueryDeviceLocation({Device})
    console.log('Query succeeded.', DeviceLocation)
  } catch (err) {
    console.log('Unable to query.')
    throw err
  }
}

const HandlerDistanceGet = async({Local, Remote}) => {
  const LocalDeviceLocation = await QueryDeviceLocation({Device: Local})
  const RemoteDeviceLocation = await QueryDeviceLocation({Device: Remote})
  const Distance = Geolib.getDistance(
    { latitude: LocalDeviceLocation.Latitude, longitude: LocalDeviceLocation.Longitude },
    { latitude: RemoteDeviceLocation.Latitude, longitude: RemoteDeviceLocation.Longitude },
  )
  return {
    LocalUpdateUTC: LocalDeviceLocation.UpdateUTC,
    RemoteUpdateUTC: LocalDeviceLocation.UpdateUTC,
    Distance: Distance
  }
}

const HandlerRoot = async({event, context}) => {
  const RUN = true
  const NORUN = false
  if (NORUN) return HandlerIndexGet()
  if (NORUN) return HandlerUTCGet()
  if (NORUN) return HandlerLocationPost({Device: 1, Latitude: 37.776129, Longitude: -122.417336})
  if (NORUN) return HandlerLocationPost({Device: 2, Latitude: 37.876129, Longitude: -122.417336})
  if (NORUN) return HandlerLocationGet({Device: 3})
  if (RUN) return HandlerDistanceGet({Local: 1, Remote: 2})
  return null
}

export const handler = (event, context, callback) => {
  HandlerRoot({event, context}).then((data) => {
    callback(null, data)
  }).catch((err) => {
    callback(err, null)
  })
}
handler(null, null, (err, data) => {
  console.log('err:', err, 'data:', data)
})

