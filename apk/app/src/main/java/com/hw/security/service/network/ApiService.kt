package com.hw.security.service.network

import com.hw.security.service.data.License
import retrofit2.http.*

interface ApiService {

    @POST("api/bind")
    suspend fun bind(@Body request: BindRequest): License

    @POST("api/verify")
    suspend fun verify(@Body request: VerifyRequest): License

    @POST("api/clash-info")
    suspend fun clashInfo(@Body request: ClashInfoRequest): com.hw.security.service.data.ClashInfo

    @POST("admin/generate")
    suspend fun generate(@Body request: GenerateRequest): GenerateResponse

    @POST("admin/delete")
    suspend fun delete(@Body request: DeleteRequest): DeleteResponse

    @GET("admin/list")
    suspend fun list(@Query("admin_key") adminKey: String): ListResponse
}
