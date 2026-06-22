package com.hw.security.service.root

import com.google.gson.Gson
import com.hw.security.service.data.License
import com.hw.security.service.util.Constants
import com.hw.security.service.util.Logger

object ModuleFileIO {

    private val gson = Gson()

    fun readLicense(): License? {
        val json = ShellExecutor.readFile(Constants.LICENSE_FILE)
        if (json.isBlank()) return null
        return try {
            gson.fromJson(json, License::class.java)
        } catch (e: Exception) {
            Logger.e("readLicense parse failed", e)
            null
        }
    }

    fun readLicenseRaw(): String {
        return ShellExecutor.readFile(Constants.LICENSE_FILE)
    }

    fun writeLicense(license: License): Boolean {
        val json = gson.toJson(license)
        return ShellExecutor.writeFile(Constants.LICENSE_FILE, json)
    }

    fun deleteLicense(): Boolean {
        return ShellExecutor.deleteFile(Constants.LICENSE_FILE)
    }

    fun readWhitelist(): List<String> {
        val content = ShellExecutor.readFile(Constants.WHITELIST_FILE)
        if (content.isBlank()) return emptyList()
        return content.lines()
            .map { it.trim() }
            .filter { it.isNotEmpty() && !it.startsWith("#") }
    }

    fun writeWhitelist(packages: List<String>): Boolean {
        val content = packages.joinToString("\n")
        return ShellExecutor.writeFile(Constants.WHITELIST_FILE, content)
    }

    fun readFreezeList(): List<String> {
        val content = ShellExecutor.readFile(Constants.FREEZE_LIST_FILE)
        if (content.isBlank()) return emptyList()
        return content.lines()
            .map { it.trim() }
            .filter { it.isNotEmpty() && !it.startsWith("#") }
    }

    fun writeFreezeList(packages: List<String>): Boolean {
        val content = packages.joinToString("\n")
        return ShellExecutor.writeFile(Constants.FREEZE_LIST_FILE, content)
    }

    fun readSerial(): String {
        return ShellExecutor.readFile(Constants.SERIAL_FILE).trim()
    }

    fun writeSerial(serial: String): Boolean {
        return ShellExecutor.writeFile(Constants.SERIAL_FILE, serial)
    }

    fun hasLicense(): Boolean {
        return ShellExecutor.fileExists(Constants.LICENSE_FILE)
    }
}
